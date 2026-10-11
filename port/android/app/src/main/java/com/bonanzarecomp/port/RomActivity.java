package com.bonanzarecomp.port;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.security.MessageDigest;

/**
 * Launcher: the game ROM is not part of the app. On first start the player picks their
 * own copy of the Bonanza Bros. ROM; it is checked and copied into the app's private
 * storage, where the native code finds it (SDL_GetPrefPath).
 */
public class RomActivity extends Activity {
    private static final String ROM_SHA1 = "f7313ecaf7143873597e3a6ebf58d542d06c3ef3";
    private static final int ROM_SIZE = 0x80000;
    private static final int PICK_ROM = 1;
    private TextView message;

    private File romFile() {
        return new File(getFilesDir(), "baserom.md");
    }

    private static String sha1(byte[] data) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-1");
        StringBuilder sb = new StringBuilder();
        for (byte b : md.digest(data))
            sb.append(String.format("%02x", b));
        return sb.toString();
    }

    private static byte[] readAll(InputStream in, int limit) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[65536];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
            if (out.size() > limit)
                break;
        }
        in.close();
        return out.toByteArray();
    }

    private boolean haveRom() {
        File f = romFile();
        if (!f.exists() || f.length() != ROM_SIZE)
            return false;
        try {
            return ROM_SHA1.equals(sha1(readAll(new FileInputStream(f), ROM_SIZE)));
        } catch (Exception e) {
            return false;
        }
    }

    private void startGame() {
        startActivity(new Intent(this, BonanzaActivity.class));
        finish();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (haveRom()) {
            startGame();
            return;
        }
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackgroundColor(Color.rgb(16, 18, 32));
        layout.setPadding(48, 48, 48, 48);

        TextView title = new TextView(this);
        title.setText("Bonanza Bros");
        title.setTextSize(28);
        title.setTextColor(Color.WHITE);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        message = new TextView(this);
        message.setText("Select your Bonanza Bros. (Mega Drive / Genesis, JUE) ROM file to start.\n"
                + "It is checked and stored only on this device.");
        message.setTextSize(16);
        message.setTextColor(Color.LTGRAY);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, 24, 0, 24);
        layout.addView(message);

        Button pick = new Button(this);
        pick.setText("Select ROM");
        pick.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, PICK_ROM);
        });
        layout.addView(pick);
        setContentView(layout);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_ROM || resultCode != RESULT_OK || data == null)
            return;
        Uri uri = data.getData();
        try {
            byte[] rom = readAll(getContentResolver().openInputStream(uri), ROM_SIZE);
            if (rom.length != ROM_SIZE || !ROM_SHA1.equals(sha1(rom))) {
                message.setText("That file is not the Bonanza Bros. (JUE) ROM (SHA-1 " + ROM_SHA1
                        + "). Please pick another file.");
                return;
            }
            FileOutputStream out = new FileOutputStream(romFile());
            out.write(rom);
            out.close();
            startGame();
        } catch (Exception e) {
            message.setText("Could not read the file: " + e.getMessage());
        }
    }
}
