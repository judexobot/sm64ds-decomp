package dev.judexobot.sm64ds;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

public final class RomDataRebuilderTest {
    static final String WHOLE = "8ee0452ba8ede9e01da4a484850146a8f8fb06f3e53560d23525e55a9578f6bd";
    static final String A9 = "3a0634f664bc781e63d3940f8c62bbfae834f8b6113481fbbc49d6a1470006dc";
    static final String OV = "3ea2ad3a9a5acc8af34e4107f74c23257ff67f9c0ef4111486248e32638b49b1";
    static final String BSS = "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7";

    static File write(Path root, String relative, String text) throws IOException {
        Path path = root.resolve(relative);
        Files.createDirectories(path.getParent());
        Files.write(path, text.getBytes(StandardCharsets.US_ASCII));
        return path.toFile();
    }
    public static void main(String[] ignored) throws Exception {
        Path root = Files.createTempDirectory("sm64ds-romdata-");
        write(root, "extracted/arm9_dec.bin", "DS!!\0\0\0\0");
        write(root, "extracted/overlays/overlay_0000.bin", "BMD0");
        String recipeText = "# romdata-recipe v1 " + WHOLE + " 10\n" +
                "0\t4\tarm9\t0\t" + A9 + "\n" +
                "4\t4\tov000\t0\t" + OV + "\n" +
                "8\t2\tarm9\t64\t" + BSS + "\n";
        File recipe = write(root, "kit/romdata.recipe.tsv", recipeText);
        File manifest = write(root, "kit/romdata.manifest",
                "# romdata-manifest v1 " + WHOLE + " 10\n" +
                "part\tromdata\t0\t10\t" + WHOLE + "\n");
        RomDataRebuilder.Result built = RomDataRebuilder.rebuild(root.toFile(), recipe, manifest);
        if (built.length != 10 || built.rows != 3 || !WHOLE.equals(built.sha256))
            throw new AssertionError("wrong build metadata");
        Path blob = root.resolve("build/assets/romdata.bin");
        if (!Arrays.equals(Files.readAllBytes(blob),
                new byte[] {'D', 'S', '!', '!', 'B', 'M', 'D', '0', 0, 0}))
            throw new AssertionError("ROM data bytes differ from recipe");
        if (!Files.isRegularFile(root.resolve("build/assets/romdata.ready")))
            throw new AssertionError("ready marker missing");
        // Corrupted source data MUST fail the row SHA and clear the ready flag.
        write(root, "extracted/overlays/overlay_0000.bin", "BAD!");
        try {
            RomDataRebuilder.rebuild(root.toFile(), recipe, manifest);
            throw new AssertionError("corrupt overlay was accepted");
        } catch (IOException expected) {
            if (!expected.getMessage().contains("SHA mismatch")) throw expected;
        }
        if (Files.exists(root.resolve("build/assets/romdata.ready")))
            throw new AssertionError("stale ready marker on failed import");
        // A recipe cannot point outside the private extraction directory.
        write(root, "extracted/overlays/overlay_0000.bin", "BMD0");
        File badRecipe = write(root, "kit/bad.recipe",
                recipeText.replace("ov000", "../../etc/passwd"));
        try {
            RomDataRebuilder.rebuild(root.toFile(), badRecipe, manifest);
            throw new AssertionError("path traversal accepted");
        } catch (IOException expected) { /* invalid source rejected */ }
        System.out.println("PC romdata recipe: SHA-256, exact byte assembly, BSS and guards passed");
    }
}
