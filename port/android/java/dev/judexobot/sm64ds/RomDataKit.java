package dev.judexobot.sm64ds;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;

/** App-bundled metadata, staged privately as one pair for the existing rebuilder. */
final class RomDataKit implements AutoCloseable {
    interface Source { InputStream open(String name) throws IOException; }
    final File recipe;
    final File manifest;
    private final File directory;

    private RomDataKit(File directory) {
        this.directory = directory;
        recipe = new File(directory, "romdata.recipe.tsv");
        manifest = new File(directory, "romdata.manifest");
    }

    static RomDataKit open(File privateFolder, Source source) throws IOException {
        if (!privateFolder.isDirectory() && !privateFolder.mkdirs())
            throw new IOException("Cannot create private ROM preparation folder");
        RomDataKit kit = new RomDataKit(Files.createTempDirectory(
                privateFolder.toPath(), "romdata-kit-").toFile());
        try {
            copy(source, kit.recipe, 8L * 1024 * 1024);
            copy(source, kit.manifest, 16L * 1024 * 1024);
            return kit;
        } catch (IOException | RuntimeException failure) {
            try { kit.close(); } catch (IOException cleanup) { failure.addSuppressed(cleanup); }
            throw failure;
        }
    }

    private static void copy(Source source, File target, long limit) throws IOException {
        try (InputStream input = source.open(target.getName());
             FileOutputStream output = new FileOutputStream(target)) {
            if (input == null) throw new IOException("Missing app ROM preparation metadata");
            byte[] buffer = new byte[65536];
            long total = 0;
            int count;
            while ((count = input.read(buffer)) != -1) {
                total += count;
                if (total > limit) throw new IOException("App ROM preparation metadata exceeds limit");
                output.write(buffer, 0, count);
            }
            if (total == 0) throw new IOException("Empty app ROM preparation metadata");
            output.getFD().sync();
        }
    }

    @Override public void close() throws IOException {
        try {
            Files.deleteIfExists(recipe.toPath());
        } finally {
            try { Files.deleteIfExists(manifest.toPath()); }
            finally { Files.deleteIfExists(directory.toPath()); }
        }
    }
}
