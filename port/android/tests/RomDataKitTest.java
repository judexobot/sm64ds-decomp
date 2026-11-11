package dev.judexobot.sm64ds;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

public final class RomDataKitTest {
    private static InputStream text(String value) {
        return new ByteArrayInputStream(value.getBytes(StandardCharsets.US_ASCII));
    }
    private static void noStagingFiles(Path folder) throws IOException {
        try (java.util.stream.Stream<Path> children = Files.list(folder)) {
            if (children.findAny().isPresent()) throw new AssertionError("metadata staging leaked");
        }
    }
    public static void main(String[] ignored) throws Exception {
        Path root = Files.createTempDirectory("sm64ds-private-import-");
        Path staging = root.resolve("metadata");
        RomDataRebuilderTest.write(root, "extracted/arm9_dec.bin", "DS!!\0\0\0\0");
        RomDataRebuilderTest.write(root, "extracted/overlays/overlay_0000.bin", "BMD0");
        String recipe = "# romdata-recipe v1 " + RomDataRebuilderTest.WHOLE + " 10\n" +
                "0\t4\tarm9\t0\t" + RomDataRebuilderTest.A9 + "\n" +
                "4\t4\tov000\t0\t" + RomDataRebuilderTest.OV + "\n" +
                "8\t2\tarm9\t64\t" + RomDataRebuilderTest.BSS + "\n";
        String manifest = "# romdata-manifest v1 " + RomDataRebuilderTest.WHOLE + " 10\n";
        try (RomDataKit kit = RomDataKit.open(staging.toFile(), name -> {
            if ("romdata.recipe.tsv".equals(name)) return text(recipe);
            if ("romdata.manifest".equals(name)) return text(manifest);
            throw new AssertionError("unapproved app asset: " + name);
        })) {
            RomDataRebuilder.Result built = RomDataRebuilder.rebuild(root.toFile(), kit.recipe, kit.manifest);
            if (built.length != 10 || !RomDataRebuilderTest.WHOLE.equals(built.sha256))
                throw new AssertionError("automatic kit differs from original rebuild");
        }
        noStagingFiles(staging);
        final boolean[] closed = {false};
        try {
            RomDataKit.open(staging.toFile(), name -> {
                if ("romdata.manifest".equals(name)) throw new IOException("incomplete build");
                return new ByteArrayInputStream(new byte[] {1}) {
                    @Override public void close() { closed[0] = true; }
                };
            });
            throw new AssertionError("missing build metadata accepted");
        } catch (IOException expected) { }
        if (!closed[0]) throw new AssertionError("app asset stream not closed");
        noStagingFiles(staging);
        try {
            RomDataKit.open(staging.toFile(), name -> new ByteArrayInputStream(new byte[0]));
            throw new AssertionError("empty metadata accepted");
        } catch (IOException expected) { }
        noStagingFiles(staging);
        System.out.println("App-owned metadata -> private ROM reconstruction and failure cleanup PASS");
    }
}
