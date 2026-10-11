package com.bonanzarecomp.port;

import org.libsdl.app.SDLActivity;

/** Runs the native port (libmain.so, built from port/CMakeLists.txt). */
public class BonanzaActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
