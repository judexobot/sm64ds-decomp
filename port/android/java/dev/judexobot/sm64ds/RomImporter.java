package dev.judexobot.sm64ds;

import android.content.Context;
import android.net.Uri;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/** Copy a user-selected document URI into private app storage for NitroFS. */
public final class RomImporter {
    private static final long MAX_ROM_BYTES = 64L * 1024 * 1024;
    private RomImporter() {}

    /**
     * Complete the ROM-clean asset step with metadata from the EXACT PC build.
     * The matching recipe/manifest contain offsets and checksums only, no ROM
     * content; they must be supplied with the final app build.
     * Invoke from a worker thread, not Activity.onCreate().
     */
    public static RomDataRebuilder.Result finishPcAssets(
            Context context, File matchingRecipe, File matchingManifest)
            throws IOException {
        File root = new File(new File(context.getFilesDir(), "game"), "pc-assets");
        RomDataRebuilder.Result result =
                RomDataRebuilder.rebuild(root, matchingRecipe, matchingManifest);
        if (!NativeBridge.nativeSetAssetRoot(root.getAbsolutePath()))
            throw new IOException("Cannot configure native PC asset root");
        return result;
    }

    public static File importEuropeanRom(Context context, Uri document) throws IOException {
        File folder = new File(context.getFilesDir(), "game");
        if (!folder.exists() && !folder.mkdirs()) throw new IOException("Cannot create game folder");
        File temp = new File(folder, "cartridge.partial");
        File finalFile = new File(folder, "sm64ds_eur.nds");
        long bytes = 0;
        try {
            try (InputStream input = context.getContentResolver().openInputStream(document);
                 FileOutputStream output = new FileOutputStream(temp)) {
                if (input == null) throw new IOException("Cannot open selected ROM");
                byte[] buffer = new byte[65536];
                int n;
                while ((n = input.read(buffer)) != -1) {
                    bytes += n;
                    if (bytes > MAX_ROM_BYTES) throw new IOException("ROM file is too large");
                    output.write(buffer, 0, n);
                }
                output.getFD().sync();
            }
            int status = NativeBridge.nativeMountRom(temp.getAbsolutePath());
            if (status != 0) throw new IOException("ROM rejected (status " + status + "); EUR ASMP required");
            if (finalFile.exists() && !finalFile.delete()) throw new IOException("Cannot replace previous ROM");
            if (!temp.renameTo(finalFile)) throw new IOException("Cannot finalize ROM copy");
            // The PC game reads many extracted NitroFS assets and its ov0
            // handle table; simply mounting the ROM is not enough. This is
            // CPU/file-intensive: call this helper on a worker thread.
            if (NativeBridge.nativeMountRom(finalFile.getAbsolutePath()) != 0)
                throw new IOException("Cannot mount finalized ROM");
            File assets = new File(folder, "pc-assets");
            int assetStatus = NativeBridge.nativePrepareAssets(finalFile.getAbsolutePath(), assets.getAbsolutePath());
            if (assetStatus != 0)
                throw new IOException("PC asset extraction failed (status " + assetStatus + ")");
            return finalFile;
        } finally {
            if (temp.exists()) temp.delete();
        }
    }
}
