#pragma once

// Hardware checks that run once at boot in the diagnostic builds (color-check,
// speaker-test and mic-test in platformio.ini). The regular firmware compiles
// none of them, and runDiagnostics() does nothing there.
void runDiagnostics();
