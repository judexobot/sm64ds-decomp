package dev.judexobot.sm64ds;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Locale;

/**
 * Build the real PC ROM-clean data file using the official kit's
 * romdata.recipe.tsv and romdata.manifest, both data-free metadata.
 *
 * The actual ARM9/overlay source images must already have been extracted
 * from the user's own NDS cartridge by nativePrepareAssets(). All bytes
 * remain in private app storage. A full-build-matched recipe is mandatory:
 * wrong ROM/revision/recipe refuses by SHA-256 rather than corrupting game
 * tables or silently launching a different game.
 *
 * Performs significant disk IO and SHA-256: invoke OFF the Android UI thread.
 * This is not a replacement for linking the game code into Android.
 */
public final class RomDataRebuilder {
    private static final long MAX_RECIPE_BYTES = 8L * 1024 * 1024;
    private static final long MAX_MANIFEST_BYTES = 16L * 1024 * 1024;
    private static final long MAX_IMAGE_BYTES = 64L * 1024 * 1024;
    private static final int MAX_BLOB_BYTES = 128 * 1024 * 1024;
    private static final int MAX_ROWS = 100000;

    private static final Pattern RECIPE_HEADER = Pattern.compile(
            "^# romdata-recipe v1 ([0-9a-f]{64}) ([0-9]+)$");
    private static final Pattern MANIFEST_HEADER = Pattern.compile(
            "^# romdata-manifest v1 ([0-9a-f]{64}) ([0-9]+)$");
    private static final Pattern SOURCE = Pattern.compile("^(?:arm9|ov[0-9]{3,4})$");
    private static final Pattern HEX = Pattern.compile("^[0-9a-f]{64}$");
    private RomDataRebuilder() {}

    public static final class Result {
        public final int length;
        public final int rows;
        public final String sha256;
        Result(int length, int rows, String sha256) {
            this.length = length;
            this.rows = rows;
            this.sha256 = sha256;
        }
    }

    private static byte[] readBounded(File file, long max) throws IOException {
        if (file == null || !file.isFile())
            throw new IOException("Missing original PC kit data: " + file);
        if (file.length() > max)
            throw new IOException("Unexpectedly large PC kit file: " + file);
        byte[] bytes = Files.readAllBytes(file.toPath());
        if (bytes.length > max)
            throw new IOException("PC kit file changed during extraction: " + file);
        return bytes;
    }

    private static String sha256(byte[] bytes, int offset, int length) {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("Android must support SHA-256", e);
        }
        digest.update(bytes, offset, length);
        byte[] hash = digest.digest();
        final char[] alphabet = "0123456789abcdef".toCharArray();
        char[] out = new char[hash.length * 2];
        for (int i = 0; i < hash.length; i++) {
            out[2 * i] = alphabet[(hash[i] >>> 4) & 15];
            out[2 * i + 1] = alphabet[hash[i] & 15];
        }
        return new String(out);
    }

    private static long parseUnsigned(String s, long max) throws IOException {
        if (!s.matches("[0-9]{1,20}"))
            throw new IOException("Malformed ROM data recipe number: " + s);
        try {
            long value = Long.parseLong(s);
            if (value < 0 || value > max) throw new NumberFormatException(s);
            return value;
        } catch (NumberFormatException exc) {
            throw new IOException("ROM data recipe value out of range: " + s, exc);
        }
    }

    private static void atomicWrite(File target, byte[] payload) throws IOException {
        File dir = target.getParentFile();
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Cannot create game data directory: " + dir);
        Path tmp = Files.createTempFile(dir.toPath(), "romdata-", ".part");
        try {
            try (FileOutputStream output = new FileOutputStream(tmp.toFile())) {
                output.write(payload);
                output.getFD().sync();
            }
            try {
                Files.move(tmp, target.toPath(),
                        StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } catch (AtomicMoveNotSupportedException unsupported) {
                Files.move(tmp, target.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(tmp);
        }
    }

    private static byte[] sourceImage(File root, String source) throws IOException {
        if (!SOURCE.matcher(source).matches())
            throw new IOException("Unsafe or unknown ROM image reference: " + source);
        final File image;
        if ("arm9".equals(source)) {
            image = new File(root, "extracted/arm9_dec.bin");
        } else {
            final int id = Integer.parseInt(source.substring(2));
            final String name = String.format(Locale.ROOT, "overlay_%04d.bin", id);
            image = new File(root, "extracted/overlays/" + name);
        }
        return readBounded(image, MAX_IMAGE_BYTES);
    }

    /**
     * @param root app-private PC asset folder, written by nativePrepareAssets
     * @param recipe exact matching PC release's romdata.recipe.tsv
     * @param manifest exact matching PC release's romdata.manifest
     */
    public static Result rebuild(File root, File recipe, File manifest) throws IOException {
        if (root == null || !root.isDirectory())
            throw new IOException("Import the user's cartridge assets first");
        final File assets = new File(root, "build/assets");
        final File ready = new File(assets, "romdata.ready");
        Files.deleteIfExists(ready.toPath());

        final byte[] recipeData = readBounded(recipe, MAX_RECIPE_BYTES);
        final byte[] manifestData = readBounded(manifest, MAX_MANIFEST_BYTES);
        final String recipeText = new String(recipeData, StandardCharsets.US_ASCII);
        final String manifestText = new String(manifestData, StandardCharsets.US_ASCII);

        BufferedReader reader = new BufferedReader(new StringReader(recipeText));
        final String header = reader.readLine();
        final Matcher match = RECIPE_HEADER.matcher(header == null ? "" : header);
        if (!match.matches()) throw new IOException("Invalid romdata.recipe.tsv header");
        final String wantedSha = match.group(1);
        final int total = (int) parseUnsigned(match.group(2), MAX_BLOB_BYTES);
        if (total == 0) throw new IOException("The ROM data recipe is empty");
        final Matcher manifestMatch = MANIFEST_HEADER.matcher(
                manifestText.split("\r?\n", 2)[0]);
        if (!manifestMatch.matches() ||
                !wantedSha.equals(manifestMatch.group(1)) ||
                total != parseUnsigned(manifestMatch.group(2), MAX_BLOB_BYTES))
            throw new IOException("romdata.manifest does not match this PC recipe");

        byte[] output = new byte[total]; // zero-filled BSS is intentional
        String cachedName = null;
        byte[] cachedImage = null;
        int position = 0, rows = 0;
        for (String line = reader.readLine(); line != null; line = reader.readLine()) {
            if (line.isEmpty() || line.startsWith("#")) continue;
            if (++rows > MAX_ROWS) throw new IOException("Recipe has too many rows");
            String[] fields = line.split("\t", -1);
            if (fields.length != 5)
                throw new IOException("Malformed ROM data recipe row " + rows);
            int destination = (int) parseUnsigned(fields[0], MAX_BLOB_BYTES);
            int length = (int) parseUnsigned(fields[1], MAX_BLOB_BYTES);
            String source = fields[2];
            long sourceOffset = parseUnsigned(fields[3], Integer.MAX_VALUE);
            String rowSha = fields[4];
            if (!SOURCE.matcher(source).matches() || !HEX.matcher(rowSha).matches() ||
                    destination != position || length > total - destination)
                throw new IOException("ROM data recipe gap, overlap or invalid hash at row " + rows);
            if (!source.equals(cachedName)) {
                cachedName = source;
                cachedImage = sourceImage(root, source);
            }
            int available = sourceOffset >= cachedImage.length ? 0 :
                    (int) Math.min((long) length, cachedImage.length - sourceOffset);
            if (available > 0)
                System.arraycopy(cachedImage, (int) sourceOffset, output, destination, available);
            if (!rowSha.equals(sha256(output, destination, length)))
                throw new IOException("PC data recipe SHA mismatch at " + source + ", row " + rows);
            position += length;
        }
        if (rows == 0 || position != total)
            throw new IOException("ROM data recipe does not cover the complete binary");
        final String gotSha = sha256(output, 0, output.length);
        if (!wantedSha.equals(gotSha))
            throw new IOException("ROM data does not match the exact retail EU cartridge / PC build");
        // Do not publish a half-generated blob: atomic rename, then manifest,
        // then ready marker. No game content enters the APK or public artifacts.
        atomicWrite(new File(assets, "romdata.bin"), output);
        atomicWrite(new File(assets, "romdata.manifest"), manifestData);
        atomicWrite(ready, (wantedSha + "\t" + total + "\n")
                .getBytes(StandardCharsets.US_ASCII));
        return new Result(total, rows, gotSha);
    }
}
