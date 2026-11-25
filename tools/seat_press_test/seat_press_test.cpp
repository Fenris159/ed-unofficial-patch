// The HMD recentre key as the seat memory reads it (src/d3d11/elite_binds.cpp eliteBindsHmdReset*): Elite's HMDReset action out of .binds
// text, slot by slot, with what watching each takes. Keyboard (with modifiers), XInput pad and HOTAS/joystick slots are observable; the
// mouse, an axis and an unnamed key are not. Nothing is opened: no device, no window, no game; the folder cases write to a scratch folder.
//   seat_press_test --self-test | --dry-run
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/d3d11/elite_binds.h"
#include "../../src/common/hotkey.h"

using namespace edvr;

namespace {
unsigned checks = 0, failures = 0;
void check(bool ok, const char* text) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", text); }
}

// A .binds file with only the HMDReset element's slots varying; the rest is the neighbours a real file has.
std::string binds(const std::string& hmd) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"Custom\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
           "\t<ResetPowerDistribution>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F11\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ResetPowerDistribution>\n" +
           hmd +
           "\t<ToggleCargoScoop>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_Home\" />\n\t\t<Secondary Device=\"231D012D\" Key=\"Joy_33\" />\n\t\t<ToggleOn Value=\"1\" />\n\t</ToggleCargoScoop>\n</Root>\n";
}
bool parse(const std::string& hmd, EliteResetBind* out) {
    const std::string text = binds(hmd);
    return eliteBindsHmdResetIn(text.data(), text.size(), out);
}

void seansBind() {
    // Sean's real file (Custom.4.2.binds, 2026-10-14): F12 on the keyboard, nothing on the secondary.
    EliteResetBind b;
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b),
          "Sean's HMDReset is found");
    check(b.present && b.count == 1, "one bound slot (the {NoDevice} secondary has no key)");
    check(b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Key && b.slot[0].binding.vk == VK_F12 && b.slot[0].binding.mods == 0,
          "the keyboard slot is F12, observable, no modifier");
    check(std::strcmp(b.slot[0].label, "Keyboard:Key_F12") == 0, "its label names the device and the key as Elite wrote them");
    char text[64];
    check(hotkeyFormatBinding(b.slot[0].binding, text, sizeof text) > 0 && std::strcmp(text, "F12") == 0, "the binding formats as the hotkey machinery's F12");
}

void keyboardAndHotas() {
    EliteResetBind b;
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_33\" />\n"
                "\t\t<Secondary Device=\"Keyboard\" Key=\"Key_F12\">\n\t\t\t<Modifier Device=\"Keyboard\" Key=\"Key_LeftControl\" />\n\t\t</Secondary>\n\t</HMDReset>\n", &b),
          "a HOTAS primary with a chorded keyboard secondary is found");
    check(b.count == 2, "two slots");
    check(b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Joy && b.slot[0].binding.joyDevice == 0x231D0200u && b.slot[0].binding.joyInput == 32,
          "the HOTAS slot is device 231D0200, Joy_33 (input 32)");
    check(std::strcmp(b.slot[0].label, "231D0200:Joy_33") == 0, "its label is the device id and the key");
    check(b.slot[1].observable && b.slot[1].binding.kind == HotkeyKind::Key && b.slot[1].binding.vk == VK_F12 && (b.slot[1].binding.mods & kHotkeyCtrl),
          "the keyboard slot is F12 with Ctrl");
    check(std::strcmp(b.slot[1].label, "Keyboard:Key_LeftControl+Key_F12") == 0 || std::strncmp(b.slot[1].label, "Keyboard:", 9) == 0, "its label names the keyboard");

    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_POV1Up\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Joy && b.slot[0].binding.joyInput == kJoyPovBase,
          "a POV hat direction is a joystick input too");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_Back\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Pad && b.slot[0].binding.padButtons != 0,
          "an XInput pad button is observable");
}

void unobservable() {
    EliteResetBind b;
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Mouse\" Key=\"Mouse_9\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.count == 1 && !b.slot[0].observable && std::strcmp(b.slot[0].label, "Mouse:Mouse_9") == 0,
          "a mouse button past the fifth is bound and is NOT observable");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Mouse\" Key=\"Mouse_XAxis\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.count == 1 && !b.slot[0].observable, "a mouse axis is bound and is NOT observable");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_XAxis\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.count == 1 && !b.slot[0].observable, "a joystick axis is bound and is NOT observable");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t\t<Secondary Device=\"231D0200\" Key=\"Joy_XAxis\" />\n\t</HMDReset>\n", &b) &&
          b.count == 2 && b.slot[0].observable && !b.slot[1].observable,
          "a key plus a joystick axis: the axis slot is the one nothing sees (the status is then unobservable for the pair)");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_NoSuchKeyAnywhere\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.count == 1 && !b.slot[0].observable, "a key this build has no name for is bound and not observable");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"{NoDevice}\" Key=\"\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) && b.present && b.count == 0,
          "an unbound HMDReset is present with no slots");
}

void mouse() {
    EliteResetBind b;
    const int expect[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
    for (int n = 1; n <= 5; ++n) {
        char slot[200];
        std::snprintf(slot, sizeof slot, "\t<HMDReset>\n\t\t<Primary Device=\"Mouse\" Key=\"Mouse_%d\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", n);
        check(parse(slot, &b) && b.count == 1 && b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Key && b.slot[0].binding.vk == expect[n - 1],
              "Mouse_1 to Mouse_5 are the left, right, middle and two side buttons as GetAsyncKeyState names them");
        check(b.slot[0].needsProof, "...and are trusted only after a first press has been seen");
        char text[64];
        HotkeyBinding back;
        check(hotkeyFormatBinding(b.slot[0].binding, text, sizeof text) > 0 && hotkeyParseBinding(text, &back, true) && hotkeyBindingsEqual(back, b.slot[0].binding),
              "...and the hotkey machinery reads the binding back as the same mouse button");
    }
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Mouse\" Key=\"Mouse_01\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) && !b.slot[0].observable,
          "a spelling that is not exactly Mouse_N is not guessed at");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) && !b.slot[0].needsProof,
          "a keyboard key needs no proof");
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_5\" />\n\t\t<Secondary Device=\"GamePad\" Key=\"GamePad_Back\" />\n\t</HMDReset>\n", &b) &&
          b.slot[0].needsProof && b.slot[1].needsProof, "a joystick and a pad button are trusted after a first press");
}

void absentAndNeighbours() {
    EliteResetBind b;
    check(!parse("", &b) && !b.present && b.count == 0, "no HMDReset element: not present");
    // The neighbours carry keys and joystick bindings of their own and must not be read as the HMD key.
    check(parse("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n", &b) &&
          b.count == 1 && b.slot[0].binding.vk == VK_F12, "ResetPowerDistribution (F11) and ToggleCargoScoop (a joystick secondary) are not the HMD key");
    EliteResetBind nul;
    check(!eliteBindsHmdResetIn(nullptr, 0, &nul) && !nul.present, "no text: not present");
    check(!eliteBindsHmdResetIn("x", 1, nullptr), "no output: false");
}

// The SRV turret view's key (Elite's ToggleBuggyTurretButton, issue 179) goes through the same parser by element name (eliteBindsActionIn); HMDReset above is the same parser with its name fixed.
bool parseTurret(const std::string& body, EliteResetBind* out) {
    const std::string text = binds(body);
    return eliteBindsActionIn("ToggleBuggyTurretButton", text.data(), text.size(), out);
}
void turretKey() {
    EliteResetBind b;
    check(parseTurret("\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_FaceUp\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ToggleBuggyTurretButton>\n", &b) &&
              b.present && b.count == 1 && b.slot[0].observable && b.slot[0].needsProof && b.slot[0].binding.kind == HotkeyKind::Pad && b.slot[0].binding.padButtons != 0 &&
              std::strcmp(b.slot[0].label, "GamePad:GamePad_FaceUp") == 0,
          "Sean's turret key (GamePad_FaceUp) is a pad button: found, observable, trusted after a first press");
    check(parseTurret("\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_T\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ToggleBuggyTurretButton>\n", &b) &&
              b.count == 1 && b.slot[0].observable && !b.slot[0].needsProof && b.slot[0].binding.kind == HotkeyKind::Key && b.slot[0].binding.vk == 'T',
          "a keyboard turret key is observable and needs no proof");
    check(parseTurret("\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_12\" />\n\t\t<Secondary Device=\"Keyboard\" Key=\"Key_F9\" />\n\t</ToggleBuggyTurretButton>\n", &b) &&
              b.count == 2 && b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Joy && b.slot[0].binding.joyDevice == 0x231D0200u && b.slot[0].binding.joyInput == 11 &&
              b.slot[1].observable && b.slot[1].binding.vk == VK_F9,
          "a joystick button with a keyboard secondary: both slots observable");
    check(parseTurret("\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Mouse\" Key=\"Mouse_7\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ToggleBuggyTurretButton>\n", &b) &&
              b.count == 1 && !b.slot[0].observable,
          "a mouse button past the fifth is bound and cannot be seen (the press falls back)");
    check(!parseTurret("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t</HMDReset>\n", &b) && !b.present && b.count == 0,
          "a file with HMDReset and no turret element: not present, and the HMD key is not read as the turret key");
    check(parseTurret("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t</HMDReset>\n\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_FaceUp\" />\n\t</ToggleBuggyTurretButton>\n", &b) &&
              b.count == 1 && b.slot[0].binding.kind == HotkeyKind::Pad, "with both elements the turret key is the turret element's, not HMDReset's");
    check(!eliteBindsActionIn(nullptr, "x", 1, &b) && !eliteBindsActionIn("", "x", 1, &b) && !eliteBindsActionIn("ToggleBuggyTurretButton", "x", 1, nullptr), "no element name, or no output: false");
}

std::wstring scratchDir() {
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    wchar_t name[64];
    std::swprintf(name, 64, L"edvr_seat_press_%lu_%llu", GetCurrentProcessId(), (unsigned long long)GetTickCount64());
    return std::wstring(tmp) + name;
}
void writeFile(const std::wstring& path, const std::string& text) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD put = 0;
    WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &put, nullptr);
    CloseHandle(f);
}

void folder() {
    const std::wstring dir = scratchDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    EliteResetBind b;
    check(!eliteBindsHmdResetDir(dir.c_str(), &b) && b.filesSeen == 0, "no preset and no .binds: not found, no file seen");
    writeFile(dir + L"\\StartPreset.4.start", "Custom\r\n");
    check(!eliteBindsHmdResetDir(dir.c_str(), &b) && b.filesSeen == 0, "a preset naming files that are not there: no file seen");
    writeFile(dir + L"\\Custom.4.2.binds", binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n"));
    check(eliteBindsHmdResetDir(dir.c_str(), &b) && b.present && b.count == 1 && b.filesSeen == 1 && std::strcmp(b.file, "Custom.4.2.binds") == 0 &&
          b.slot[0].binding.vk == VK_F12, "the active preset's file answers, and names itself");
    // The newest active file answers alone, even if an older one would have said something else.
    Sleep(30);
    writeFile(dir + L"\\Custom.4.3.binds", binds("\t<HMDReset>\n\t\t<Primary Device=\"231D0200\" Key=\"Joy_12\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n"));
    check(eliteBindsHmdResetDir(dir.c_str(), &b) && std::strcmp(b.file, "Custom.4.3.binds") == 0 && b.slot[0].binding.kind == HotkeyKind::Joy,
          "the newest active file wins (a rebind to a HOTAS button)");
    // A file for another preset is not consulted.
    Sleep(30);
    writeFile(dir + L"\\Other.4.2.binds", binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F1\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</HMDReset>\n"));
    check(eliteBindsHmdResetDir(dir.c_str(), &b) && std::strcmp(b.file, "Custom.4.3.binds") == 0, "a newer file of another preset is not the active one");
    // A file without the element: the next active file is consulted (first that CONTAINS it answers).
    Sleep(30);
    writeFile(dir + L"\\Custom.4.4.binds", binds(""));
    check(eliteBindsHmdResetDir(dir.c_str(), &b) && b.filesSeen == 2 && std::strcmp(b.file, "Custom.4.3.binds") == 0,
          "the newest file without the element is passed over for the next that has it");
    DeleteFileW((dir + L"\\StartPreset.4.start").c_str());
    DeleteFileW((dir + L"\\Custom.4.2.binds").c_str());
    DeleteFileW((dir + L"\\Custom.4.3.binds").c_str());
    DeleteFileW((dir + L"\\Custom.4.4.binds").c_str());
    DeleteFileW((dir + L"\\Other.4.2.binds").c_str());
    RemoveDirectoryW(dir.c_str());
    check(GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES, "the scratch folder is gone");
}

// The SRV's bind context (issue 179): Sean's real StartPreset.4.start is "Custom | Custom | ConsoleX360 | Custom", one preset per context, the third the SRV's. Custom.4.2.binds holds a stale
// FaceUp for the turret; the stock ConsoleX360 preset, in the game directory's ControlSchemes folder, binds LThumb.
void srvContext() {
    const std::wstring root = scratchDir() + L"_srv";
    const std::wstring opt = root + L"\\Options", game = root + L"\\Game", schemes = game + L"\\ControlSchemes";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(opt.c_str(), nullptr);
    CreateDirectoryW(game.c_str(), nullptr);
    CreateDirectoryW(schemes.c_str(), nullptr);
    const std::string face = "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_FaceUp\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ToggleBuggyTurretButton>\n";
    const std::string lthumb = "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_LThumb\" />\n\t\t<Secondary Device=\"{NoDevice}\" Key=\"\" />\n\t</ToggleBuggyTurretButton>\n";
    const std::string back = "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"GamePad\" Key=\"GamePad_Back\" />\n\t</ToggleBuggyTurretButton>\n";
    EliteResetBind b;
    writeFile(opt + L"\\Custom.4.2.binds", binds(face));
    // the old one-line file: today's answer (the Custom file)
    writeFile(opt + L"\\StartPreset.start", "Custom\r\n");
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b) && std::strcmp(b.file, "Custom.4.2.binds") == 0 && b.slot[0].binding.padButtons == 0x8000,
          "with only the old StartPreset.start the answer is the active preset's file, as before");
    writeFile(opt + L"\\StartPreset.4.start", "Custom\r\nCustom\r\n");
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b) && std::strcmp(b.file, "Custom.4.2.binds") == 0, "a StartPreset.4.start of fewer than four lines: today's answer");
    // Sean's file
    writeFile(opt + L"\\StartPreset.4.start", "Custom\r\nCustom\r\nConsoleX360\r\nCustom\r\n");
    check(!eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b), "the SRV line names ConsoleX360, which is nowhere yet: not found (the stale Custom file is not used)");
    writeFile(schemes + L"\\ConsoleX360.binds", binds(lthumb));
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b) && b.count == 1 && b.slot[0].observable && b.slot[0].binding.kind == HotkeyKind::Pad &&
              b.slot[0].binding.padButtons == 0x0040 && std::strcmp(b.file, "ControlSchemes\\ConsoleX360.binds") == 0 && std::strcmp(b.slot[0].label, "GamePad:GamePad_LThumb") == 0,
          "Custom with FaceUp beside ConsoleX360 with LThumb in ControlSchemes: the SRV line gives LThumb, and the file name is ControlSchemes\\ConsoleX360.binds");
    check(eliteBindsActionDir("ToggleBuggyTurretButton", opt.c_str(), &b) && b.slot[0].binding.padButtons == 0x8000, "...while the ordinary lookup (HMDReset's and the FSS's way) still answers FaceUp from the Custom file");
    // a maintained copy in Options\Bindings wins over the stock one
    Sleep(30);
    writeFile(opt + L"\\ConsoleX360.4.2.binds", binds(back));
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b) && std::strcmp(b.file, "ConsoleX360.4.2.binds") == 0 && b.slot[0].binding.padButtons == 0x0020,
          "a maintained ConsoleX360 file in Options\\Bindings is looked up first");
    writeFile(opt + L"\\ConsoleX360.4.2.binds", binds(""));
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), game.c_str(), &b) && std::strcmp(b.file, "ControlSchemes\\ConsoleX360.binds") == 0,
          "...and one without the element is passed over for the stock file");
    DeleteFileW((opt + L"\\StartPreset.start").c_str());
    DeleteFileW((opt + L"\\StartPreset.4.start").c_str());
    DeleteFileW((opt + L"\\Custom.4.2.binds").c_str());
    DeleteFileW((opt + L"\\ConsoleX360.4.2.binds").c_str());
    DeleteFileW((schemes + L"\\ConsoleX360.binds").c_str());
    RemoveDirectoryW(schemes.c_str());
    RemoveDirectoryW(game.c_str());
    RemoveDirectoryW(opt.c_str());
    RemoveDirectoryW(root.c_str());
    check(GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES, "the SRV scratch folder is gone");
}

// Issue #182: every lookup reads its own bind context line in StartPreset.4.start
// (General=0, Ship=1, SRV=2, OnFoot=3), not a newer preset belonging to another context.
void perContextPresets() {
    const std::wstring root = scratchDir() + L"_ctx182";
    const std::wstring opt = root + L"\\Options";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(opt.c_str(), nullptr);

    // Four distinct presets: General, Ship, SRV, OnFoot.
    writeFile(opt + L"\\StartPreset.4.start", "GenPreset\r\nShipPreset\r\nSrvPreset\r\nFootPreset\r\n");
    writeFile(opt + L"\\GenPreset.4.2.binds",
              binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F12\" />\n\t</HMDReset>\n"
                    "\t<UI_Up>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_W\" />\n\t</UI_Up>\n"));
    Sleep(30);
    writeFile(opt + L"\\ShipPreset.4.2.binds",
              binds("\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F9\" />\n"
                    "\t\t<Secondary Device=\"GamePad\" Key=\"GamePad_DPadUp\" />\n\t</ExplorationFSSEnter>\n"));
    // Make SrvPreset NEWER than both GenPreset and ShipPreset, containing stale wrong keys for them.
    Sleep(30);
    writeFile(opt + L"\\SrvPreset.4.2.binds",
              binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F1\" />\n\t</HMDReset>\n"
                    "\t<UI_Up>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_Q\" />\n\t</UI_Up>\n"
                    "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F2\" />\n"
                    "\t\t<Secondary Device=\"GamePad\" Key=\"GamePad_Back\" />\n\t</ExplorationFSSEnter>\n"
                    "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_T\" />\n\t</ToggleBuggyTurretButton>\n"));

    EliteResetBind hmd;
    check(eliteBindsHmdResetDir(opt.c_str(), &hmd) && std::strcmp(hmd.file, "GenPreset.4.2.binds") == 0 &&
              hmd.count == 1 && hmd.slot[0].binding.vk == VK_F12,
          "#182: HMDReset reads General (line 0), not a newer SRV preset");

    EliteKeySlots ui;
    check(eliteBindsLookupSlotsDir(opt.c_str(), "UI_Up", kEliteKeyAllowModifierMain, &ui) &&
              std::strcmp(ui.file, "GenPreset.4.2.binds") == 0 && ui.count == 1 &&
              std::strcmp(ui.slot[0].binding, "W") == 0,
          "#182: menu UI_Up reads General (line 0), not a newer SRV preset");

    char keyOut[32] = {};
    check(eliteBindsLookupDir(opt.c_str(), "ExplorationFSSEnter", keyOut, sizeof(keyOut)) &&
              std::strcmp(keyOut, "F9") == 0,
          "#182: ExplorationFSSEnter keyboard reads Ship (line 1), not a newer SRV preset");

    char padOut[32] = {};
    check(eliteBindsLookupPadDir(opt.c_str(), "ExplorationFSSEnter", padOut, sizeof(padOut)) &&
              std::strcmp(padOut, "GamePad_DPadUp") == 0,
          "#182: ExplorationFSSEnter gamepad reads Ship (line 1), not a newer SRV preset");

    // A 3-line StartPreset.4.start with trailing CRLF has 3 lines (not 4), so it must fall back rather than treat line 3 as an empty 4th line.
    writeFile(opt + L"\\StartPreset.4.start", "GenPreset\r\nShipPreset\r\nMissingSrv\r\n");
    EliteResetBind fallbackSrv;
    check(eliteBindsActionSrvDir("ToggleBuggyTurretButton", opt.c_str(), nullptr, &fallbackSrv) == false &&
              eliteBindsActionSrvDir("HMDReset", opt.c_str(), nullptr, &fallbackSrv) &&
              std::strcmp(fallbackSrv.file, "GenPreset.4.2.binds") == 0,
          "#182: a 3-line StartPreset.4.start with trailing CRLF falls back instead of counting a phantom 4th empty line");

    // Stock ControlSchemes lookup across General, Ship, SRV, and OnFoot + UTF-8 BOM + multi-context clash uses + traversal rejection.
    const std::wstring game = root + L"\\Game", schemes = game + L"\\ControlSchemes";
    CreateDirectoryW(game.c_str(), nullptr);
    CreateDirectoryW(schemes.c_str(), nullptr);
    writeFile(schemes + L"\\StockGen.binds",
              binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F10\" />\n\t</HMDReset>\n"
                    "\t<UI_Up>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_UpArrow\" />\n\t</UI_Up>\n"));
    writeFile(schemes + L"\\StockShip.binds",
              binds("\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_Apostrophe\" />\n"
                    "\t\t<Secondary Device=\"GamePad\" Key=\"GamePad_LeftShoulder\" />\n\t</ExplorationFSSEnter>\n"
                    "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F7\" />\n\t</ToggleBuggyTurretButton>\n"));
    writeFile(schemes + L"\\StockFoot.binds",
              binds("\t<HumanoidJumpButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_Space\" />\n\t</HumanoidJumpButton>\n"));
    writeFile(opt + L"\\StartPreset.4.start", "\xEF\xBB\xBFStockGen\r\nStockShip\r\nSrvPreset\r\nStockFoot\r\n");

    check(eliteBindsActionContextDir("HMDReset", EliteBindContext::General, opt.c_str(), game.c_str(), &hmd) &&
              std::strcmp(hmd.file, "ControlSchemes\\StockGen.binds") == 0 && hmd.slot[0].binding.vk == VK_F10,
          "#182: General HMDReset resolves stock ControlSchemes preset with UTF-8 BOM in StartPreset.4.start");
    check(eliteBindsLookupSlotsContextDir(opt.c_str(), game.c_str(), EliteBindContext::General, "UI_Up", kEliteKeyAllowModifierMain, &ui) &&
              std::strcmp(ui.file, "ControlSchemes\\StockGen.binds") == 0 && std::strcmp(ui.slot[0].binding, "UP") == 0,
          "#182: General menu UI_Up resolves stock ControlSchemes preset");
    check(eliteBindsLookupContextDir(opt.c_str(), game.c_str(), EliteBindContext::Ship, "ExplorationFSSEnter", keyOut, sizeof(keyOut)) &&
              std::strcmp(keyOut, "APOSTROPHE") == 0,
          "#182: Ship ExplorationFSSEnter keyboard resolves stock ControlSchemes preset");
    check(eliteBindsLookupPadContextDir(opt.c_str(), game.c_str(), EliteBindContext::Ship, "ExplorationFSSEnter", padOut, sizeof(padOut)) &&
              std::strcmp(padOut, "GamePad_LeftShoulder") == 0,
          "#182: Ship ExplorationFSSEnter gamepad resolves stock ControlSchemes preset");
    check(eliteBindsActionContextDir("HumanoidJumpButton", EliteBindContext::OnFoot, opt.c_str(), game.c_str(), &hmd) &&
              std::strcmp(hmd.file, "ControlSchemes\\StockFoot.binds") == 0 && hmd.slot[0].binding.vk == VK_SPACE,
          "#182: OnFoot (line 3) resolves stock ControlSchemes preset");

    // Multi-context clash checks scan every active context preset (Options\Bindings + ControlSchemes)
    // and filter out inactive-context actions (e.g. stale SRV button in StockShip, stale General/Ship keys in SrvPreset).
    EliteBindUse allUses[64];
    char usesFile[64] = {};
    const int nAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), allUses, 64, usesFile, sizeof(usesFile));
    bool sawF10 = false, sawApos = false, sawT = false, sawSpace = false, leakedInactiveAll = false;
    for (int i = 0; i < nAll; ++i) {
        if (std::strcmp(allUses[i].element, "HMDReset") == 0 && allUses[i].binding.vk == VK_F10) sawF10 = true;
        if (std::strcmp(allUses[i].element, "ExplorationFSSEnter") == 0 && allUses[i].binding.vk == VK_OEM_7) sawApos = true;
        if (std::strcmp(allUses[i].element, "ToggleBuggyTurretButton") == 0 && allUses[i].binding.vk == 'T') sawT = true;
        if (std::strcmp(allUses[i].element, "HumanoidJumpButton") == 0 && allUses[i].binding.vk == VK_SPACE) sawSpace = true;
        if ((std::strcmp(allUses[i].element, "ToggleBuggyTurretButton") == 0 && allUses[i].binding.vk == VK_F7) ||
            (std::strcmp(allUses[i].element, "HMDReset") == 0 && allUses[i].binding.vk == VK_F1) ||
            (std::strcmp(allUses[i].element, "UI_Up") == 0 && allUses[i].binding.vk == 'Q') ||
            (std::strcmp(allUses[i].element, "ExplorationFSSEnter") == 0 && allUses[i].binding.vk == VK_F2)) {
            leakedInactiveAll = true;
        }
    }
    check(nAll > 0 && sawF10 && sawApos && sawT && sawSpace && !leakedInactiveAll && std::strstr(usesFile, "+3 more") != nullptr,
          "#182: eliteBindsAllUsesDirEx scans all four active context presets and excludes inactive-context stale bindings");

    EliteKeyboardUse kbUses[64];
    const int nKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), kbUses, 64, nullptr, 0);
    bool kbSawSpace = false, kbSawT = false, leakedInactiveKb = false;
    for (int i = 0; i < nKb; ++i) {
        if (std::strcmp(kbUses[i].element, "HumanoidJumpButton") == 0 && std::strcmp(kbUses[i].binding, "SPACE") == 0) kbSawSpace = true;
        if (std::strcmp(kbUses[i].element, "ToggleBuggyTurretButton") == 0 && std::strcmp(kbUses[i].binding, "T") == 0) kbSawT = true;
        if ((std::strcmp(kbUses[i].element, "ToggleBuggyTurretButton") == 0 && std::strcmp(kbUses[i].binding, "F7") == 0) ||
            (std::strcmp(kbUses[i].element, "HMDReset") == 0 && std::strcmp(kbUses[i].binding, "F1") == 0) ||
            (std::strcmp(kbUses[i].element, "ExplorationFSSEnter") == 0 && std::strcmp(kbUses[i].binding, "F2") == 0)) {
            leakedInactiveKb = true;
        }
    }
    check(nKb > 0 && kbSawSpace && kbSawT && !leakedInactiveKb,
          "#182: eliteBindsKeyboardUsesDirEx scans active presets and excludes inactive-context stale bindings");

    // Pre-dedup truncation regression (#182): when file 1 uses max-1 slots and file 2 repeats file 1's entries
    // before a unique SRV binding at the end, deduplication must not drop the unique binding in file 2.
    writeFile(opt + L"\\DupBase.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"DupBase\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F1\" />\n\t</HMDReset>\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F2\" />\n\t</ExplorationFSSEnter>\n"
              "\t<UI_Up>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F3\" />\n\t</UI_Up>\n</Root>\n");
    writeFile(opt + L"\\DupSrv.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"DupSrv\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F1\" />\n\t</HMDReset>\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F2\" />\n\t</ExplorationFSSEnter>\n"
              "\t<UI_Up>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F3\" />\n\t</UI_Up>\n"
              "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_T\" />\n\t</ToggleBuggyTurretButton>\n</Root>\n");
    writeFile(opt + L"\\StartPreset.4.start", "DupBase\r\nDupBase\r\nDupSrv\r\nDupBase\r\n");
    EliteBindUse tightAll[4];
    const int nTightAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), tightAll, 4, nullptr, 0);
    bool tightAllSawT = false;
    for (int i = 0; i < nTightAll; ++i) {
        if (std::strcmp(tightAll[i].element, "ToggleBuggyTurretButton") == 0 && tightAll[i].binding.vk == 'T') tightAllSawT = true;
    }
    EliteKeyboardUse tightKb[4];
    const int nTightKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), tightKb, 4, nullptr, 0);
    bool tightKbSawT = false;
    for (int i = 0; i < nTightKb; ++i) {
        if (std::strcmp(tightKb[i].element, "ToggleBuggyTurretButton") == 0 && std::strcmp(tightKb[i].binding, "T") == 0) tightKbSawT = true;
    }
    check(nTightAll == 4 && tightAllSawT && nTightKb == 4 && tightKbSawT,
          "#182: duplicate entries in secondary preset do not consume max-total scan budget before unique SRV binding");
    DeleteFileW((opt + L"\\DupBase.4.2.binds").c_str());
    DeleteFileW((opt + L"\\DupSrv.4.2.binds").c_str());

    // Blank context line fallback parity (#182): when a 4-line StartPreset.4.start has a blank
    // Ship line while other lines name a preset, direct Ship lookup falls back to the unique named
    // presets and both clash APIs must include Ship actions from that same fallback file.
    writeFile(opt + L"\\BlankFallback.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"BlankFallback\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F1\" />\n\t</HMDReset>\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F6\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    writeFile(opt + L"\\StartPreset.4.start", "BlankFallback\r\n\r\nBlankFallback\r\nBlankFallback\r\n");
    EliteResetBind blankShipBind{};
    const bool blankLookupOk = eliteBindsActionContextDir("ExplorationFSSEnter", EliteBindContext::Ship,
                                                          opt.c_str(), game.c_str(), &blankShipBind);
    EliteBindUse blankAll[16];
    const int nBlankAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), blankAll, 16, nullptr, 0);
    bool blankAllSawFss = false;
    for (int i = 0; i < nBlankAll; ++i) {
        if (std::strcmp(blankAll[i].element, "ExplorationFSSEnter") == 0 && blankAll[i].binding.vk == VK_F6) blankAllSawFss = true;
    }
    EliteKeyboardUse blankKb[16];
    const int nBlankKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), blankKb, 16, nullptr, 0);
    bool blankKbSawFss = false;
    for (int i = 0; i < nBlankKb; ++i) {
        if (std::strcmp(blankKb[i].element, "ExplorationFSSEnter") == 0 && std::strcmp(blankKb[i].binding, "F6") == 0) blankKbSawFss = true;
    }
    check(blankLookupOk && blankShipBind.count == 1 && blankShipBind.slot[0].binding.vk == VK_F6 &&
              blankAllSawFss && blankKbSawFss,
          "#182: blank Ship line in 4-line StartPreset.4.start uses same preset fallback for direct lookup and both clash APIs");
    DeleteFileW((opt + L"\\BlankFallback.4.2.binds").c_str());

    // Per-action fallback parity (#182):
    // 1. When the newest file for a preset omits ExplorationFSSEnter while an older file (or stock
    //    ControlSchemes) defines it, direct lookup and both clash APIs all pick up ExplorationFSSEnter.
    // 2. When the newest file explicitly defines ExplorationFSSEnter as {NoDevice}, neither direct
    //    lookup nor either clash API resurrects the older file's binding.
    auto setMtimeSec = [](const std::wstring& path, ULONGLONG sec) {
        HANDLE h = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        ULARGE_INTEGER ui{};
        ui.QuadPart = 133000000000000000ULL + sec * 10000000ULL;
        FILETIME ft{ui.LowPart, ui.HighPart};
        SetFileTime(h, nullptr, nullptr, &ft);
        CloseHandle(h);
    };
    writeFile(opt + L"\\FallPreset.4.1.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"FallPreset\" MajorVersion=\"4\" MinorVersion=\"1\">\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F6\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    writeFile(opt + L"\\FallPreset.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"FallPreset\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ResetPowerDistribution>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F11\" />\n\t</ResetPowerDistribution>\n</Root>\n");
    setMtimeSec(opt + L"\\FallPreset.4.1.binds", 10);
    setMtimeSec(opt + L"\\FallPreset.4.2.binds", 20);
    writeFile(opt + L"\\StartPreset.4.start", "StockGen\r\nFallPreset\r\nSrvPreset\r\nStockFoot\r\n");
    EliteResetBind fallShipBind{};
    const bool fallLookupOk = eliteBindsActionContextDir("ExplorationFSSEnter", EliteBindContext::Ship,
                                                         opt.c_str(), game.c_str(), &fallShipBind);
    EliteBindUse fallAll[32];
    const int nFallAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), fallAll, 32, nullptr, 0);
    bool fallAllSawFss = false;
    for (int i = 0; i < nFallAll; ++i) {
        if (std::strcmp(fallAll[i].element, "ExplorationFSSEnter") == 0 && fallAll[i].binding.vk == VK_F6) fallAllSawFss = true;
    }
    EliteKeyboardUse fallKb[32];
    const int nFallKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), fallKb, 32, nullptr, 0);
    bool fallKbSawFss = false;
    for (int i = 0; i < nFallKb; ++i) {
        if (std::strcmp(fallKb[i].element, "ExplorationFSSEnter") == 0 && std::strcmp(fallKb[i].binding, "F6") == 0) fallKbSawFss = true;
    }
    check(fallLookupOk && fallShipBind.count == 1 && fallShipBind.slot[0].binding.vk == VK_F6 &&
              fallAllSawFss && fallKbSawFss,
          "#182: when newest preset file omits an action, direct lookup and both clash APIs fall through to older file");

    writeFile(opt + L"\\FallPreset.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"FallPreset\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ResetPowerDistribution>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F11\" />\n\t</ResetPowerDistribution>\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"{NoDevice}\" Key=\"\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    setMtimeSec(opt + L"\\FallPreset.4.2.binds", 30);
    EliteResetBind clearedShipBind{};
    const bool clearedLookupOk = eliteBindsActionContextDir("ExplorationFSSEnter", EliteBindContext::Ship,
                                                            opt.c_str(), game.c_str(), &clearedShipBind);
    const int nClearedAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), fallAll, 32, nullptr, 0);
    bool clearedAllSawFss = false;
    for (int i = 0; i < nClearedAll; ++i) {
        if (std::strcmp(fallAll[i].element, "ExplorationFSSEnter") == 0) clearedAllSawFss = true;
    }
    const int nClearedKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), fallKb, 32, nullptr, 0);
    bool clearedKbSawFss = false;
    for (int i = 0; i < nClearedKb; ++i) {
        if (std::strcmp(fallKb[i].element, "ExplorationFSSEnter") == 0) clearedKbSawFss = true;
    }
    check(clearedLookupOk && clearedShipBind.present && clearedShipBind.count == 0 &&
              std::strcmp(clearedShipBind.file, "FallPreset.4.2.binds") == 0 &&
              !eliteBindsLookupContextDir(opt.c_str(), game.c_str(), EliteBindContext::Ship,
                                          "ExplorationFSSEnter", keyOut, sizeof(keyOut)) &&
              !clearedAllSawFss && !clearedKbSawFss,
          "#182: explicit {NoDevice} action in newest preset file blocks older file in direct lookup and both clash APIs");
    DeleteFileW((opt + L"\\FallPreset.4.1.binds").c_str());
    DeleteFileW((opt + L"\\FallPreset.4.2.binds").c_str());

    // Split-preset General vs Ship HeadlookToggle check (#182): Elite spells <HeadlookToggle>
    // with lowercase 'l' and stores it in General (line 0), so clash scans must report General's
    // HeadlookToggle and ignore a stale one left in Ship's .binds file.
    writeFile(opt + L"\\HeadlookGen.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"HeadlookGen\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<HeadlookToggle>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_H\" />\n\t</HeadlookToggle>\n</Root>\n");
    writeFile(opt + L"\\HeadlookShip.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"HeadlookShip\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<HeadlookToggle>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_J\" />\n\t</HeadlookToggle>\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F6\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    writeFile(opt + L"\\StartPreset.4.start", "HeadlookGen\r\nHeadlookShip\r\nSrvPreset\r\nStockFoot\r\n");
    EliteBindUse hlAll[32];
    const int nHlAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), hlAll, 32, nullptr, 0);
    bool hlAllHasGen = false, hlAllHasShip = false;
    for (int i = 0; i < nHlAll; ++i) {
        if (std::strcmp(hlAll[i].element, "HeadlookToggle") == 0) {
            if (hlAll[i].binding.vk == 'H') hlAllHasGen = true;
            if (hlAll[i].binding.vk == 'J') hlAllHasShip = true;
        }
    }
    EliteKeyboardUse hlKb[32];
    const int nHlKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), hlKb, 32, nullptr, 0);
    bool hlKbHasGen = false, hlKbHasShip = false;
    for (int i = 0; i < nHlKb; ++i) {
        if (std::strcmp(hlKb[i].element, "HeadlookToggle") == 0) {
            if (std::strcmp(hlKb[i].binding, "H") == 0) hlKbHasGen = true;
            if (std::strcmp(hlKb[i].binding, "J") == 0) hlKbHasShip = true;
        }
    }
    check(hlAllHasGen && !hlAllHasShip && hlKbHasGen && !hlKbHasShip,
          "#182: split-preset clash scans read HeadlookToggle from General (line 0) and ignore Ship's stale HeadlookToggle");
    DeleteFileW((opt + L"\\HeadlookGen.4.2.binds").c_str());
    DeleteFileW((opt + L"\\HeadlookShip.4.2.binds").c_str());

    // Split-preset General UIFocus/PhotoCameraToggle and SRV IncreaseSpeedButtonPartial check (#182):
    // UIFocus and PhotoCameraToggle live in General (line 0) and must not be dropped when General and Ship
    // use different presets, while Ship's stale UIFocus is ignored and SRV's partial-speed bind is read from line 2.
    writeFile(opt + L"\\SplitGen.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"SplitGen\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<UIFocus>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F9\" />\n\t</UIFocus>\n"
              "\t<PhotoCameraToggle>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F10\" />\n\t</PhotoCameraToggle>\n"
              "</Root>\n");
    writeFile(opt + L"\\SplitShip.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"SplitShip\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<UIFocus>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F11\" />\n\t</UIFocus>\n"
              "\t<PrimaryFire>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_Space\" />\n\t</PrimaryFire>\n"
              "</Root>\n");
    writeFile(opt + L"\\SplitSrv.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"SplitSrv\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<IncreaseSpeedButtonPartial>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_K\" />\n\t</IncreaseSpeedButtonPartial>\n"
              "</Root>\n");
    writeFile(opt + L"\\StartPreset.4.start", "SplitGen\r\nSplitShip\r\nSplitSrv\r\nStockFoot\r\n");
    EliteBindUse spAll[32];
    const int nSpAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), spAll, 32, nullptr, 0);
    bool spAllUiGen = false, spAllUiShip = false, spAllPhoto = false, spAllSrvSpeed = false;
    for (int i = 0; i < nSpAll; ++i) {
        if (std::strcmp(spAll[i].element, "UIFocus") == 0) {
            if (spAll[i].binding.vk == VK_F9) spAllUiGen = true;
            if (spAll[i].binding.vk == VK_F11) spAllUiShip = true;
        }
        if (std::strcmp(spAll[i].element, "PhotoCameraToggle") == 0 && spAll[i].binding.vk == VK_F10) spAllPhoto = true;
        if (std::strcmp(spAll[i].element, "IncreaseSpeedButtonPartial") == 0 && spAll[i].binding.vk == 'K') spAllSrvSpeed = true;
    }
    EliteKeyboardUse spKb[32];
    const int nSpKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), spKb, 32, nullptr, 0);
    bool spKbUiGen = false, spKbUiShip = false, spKbPhoto = false, spKbSrvSpeed = false;
    for (int i = 0; i < nSpKb; ++i) {
        if (std::strcmp(spKb[i].element, "UIFocus") == 0) {
            if (std::strcmp(spKb[i].binding, "F9") == 0) spKbUiGen = true;
            if (std::strcmp(spKb[i].binding, "F11") == 0) spKbUiShip = true;
        }
        if (std::strcmp(spKb[i].element, "PhotoCameraToggle") == 0 && std::strcmp(spKb[i].binding, "F10") == 0) spKbPhoto = true;
        if (std::strcmp(spKb[i].element, "IncreaseSpeedButtonPartial") == 0 && std::strcmp(spKb[i].binding, "K") == 0) spKbSrvSpeed = true;
    }
    check(spAllUiGen && !spAllUiShip && spAllPhoto && spAllSrvSpeed &&
              spKbUiGen && !spKbUiShip && spKbPhoto && spKbSrvSpeed,
          "#182: split-preset clash scans preserve General UIFocus/PhotoCameraToggle and SRV IncreaseSpeedButtonPartial");
    DeleteFileW((opt + L"\\SplitGen.4.2.binds").c_str());
    DeleteFileW((opt + L"\\SplitShip.4.2.binds").c_str());
    DeleteFileW((opt + L"\\SplitSrv.4.2.binds").c_str());

    // Dotted sibling preset stem isolation (#182): preset "Pilot" must match only
    // Pilot.binds / Pilot.<digits>(.<digits>)*.binds and NEVER claim newer sibling presets
    // like Pilot.SRV.4.2.binds or Pilot.backup.binds, including when Pilot falls through to
    // stock ControlSchemes\Pilot.binds.
    writeFile(opt + L"\\Pilot.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"Pilot\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F5\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    writeFile(opt + L"\\Pilot.SRV.4.2.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"Pilot.SRV\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F6\" />\n\t</ExplorationFSSEnter>\n"
              "\t<ToggleBuggyTurretButton>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_T\" />\n\t</ToggleBuggyTurretButton>\n</Root>\n");
    writeFile(opt + L"\\Pilot.backup.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"Pilot.backup\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F7\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    setMtimeSec(opt + L"\\Pilot.4.2.binds", 10);
    setMtimeSec(opt + L"\\Pilot.SRV.4.2.binds", 20);
    setMtimeSec(opt + L"\\Pilot.backup.binds", 30);
    writeFile(opt + L"\\StartPreset.4.start", "StockGen\r\nPilot\r\nPilot.SRV\r\nStockFoot\r\n");
    EliteResetBind pilotShipBind{}, pilotSrvBind{};
    const bool pilotShipOk = eliteBindsActionContextDir("ExplorationFSSEnter", EliteBindContext::Ship,
                                                        opt.c_str(), game.c_str(), &pilotShipBind);
    const bool pilotSrvOk = eliteBindsActionContextDir("ToggleBuggyTurretButton", EliteBindContext::Srv,
                                                       opt.c_str(), game.c_str(), &pilotSrvBind);
    EliteBindUse pilotAll[32];
    const int nPilotAll = eliteBindsAllUsesDirEx(opt.c_str(), game.c_str(), pilotAll, 32, nullptr, 0);
    bool pilotAllF5 = false, pilotAllBad = false, pilotAllSrvT = false;
    for (int i = 0; i < nPilotAll; ++i) {
        if (std::strcmp(pilotAll[i].element, "ExplorationFSSEnter") == 0) {
            if (pilotAll[i].binding.vk == VK_F5) pilotAllF5 = true;
            if (pilotAll[i].binding.vk == VK_F6 || pilotAll[i].binding.vk == VK_F7) pilotAllBad = true;
        }
        if (std::strcmp(pilotAll[i].element, "ToggleBuggyTurretButton") == 0 && pilotAll[i].binding.vk == 'T') {
            pilotAllSrvT = true;
        }
    }
    EliteKeyboardUse pilotKb[32];
    const int nPilotKb = eliteBindsKeyboardUsesDirEx(opt.c_str(), game.c_str(), pilotKb, 32, nullptr, 0);
    bool pilotKbF5 = false, pilotKbBad = false, pilotKbSrvT = false;
    for (int i = 0; i < nPilotKb; ++i) {
        if (std::strcmp(pilotKb[i].element, "ExplorationFSSEnter") == 0) {
            if (std::strcmp(pilotKb[i].binding, "F5") == 0) pilotKbF5 = true;
            if (std::strcmp(pilotKb[i].binding, "F6") == 0 || std::strcmp(pilotKb[i].binding, "F7") == 0) pilotKbBad = true;
        }
        if (std::strcmp(pilotKb[i].element, "ToggleBuggyTurretButton") == 0 && std::strcmp(pilotKb[i].binding, "T") == 0) {
            pilotKbSrvT = true;
        }
    }
    check(pilotShipOk && pilotShipBind.count == 1 && pilotShipBind.slot[0].binding.vk == VK_F5 &&
              std::strcmp(pilotShipBind.file, "Pilot.4.2.binds") == 0 &&
              pilotSrvOk && pilotSrvBind.count == 1 && pilotSrvBind.slot[0].binding.vk == 'T' &&
              std::strcmp(pilotSrvBind.file, "Pilot.SRV.4.2.binds") == 0 &&
              pilotAllF5 && !pilotAllBad && pilotAllSrvT &&
              pilotKbF5 && !pilotKbBad && pilotKbSrvT,
          "#182: preset stem matching isolates Pilot from newer Pilot.SRV.4.2.binds and Pilot.backup.binds");

    // When Pilot.4.2.binds is absent, Ship falls through to stock ControlSchemes\Pilot.binds
    // rather than matching Pilot.SRV.4.2.binds.
    DeleteFileW((opt + L"\\Pilot.4.2.binds").c_str());
    writeFile(schemes + L"\\Pilot.binds",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<Root PresetName=\"Pilot\" MajorVersion=\"4\" MinorVersion=\"2\">\n"
              "\t<ExplorationFSSEnter>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F4\" />\n\t</ExplorationFSSEnter>\n</Root>\n");
    EliteResetBind stockPilotBind{};
    const bool stockPilotOk = eliteBindsActionContextDir("ExplorationFSSEnter", EliteBindContext::Ship,
                                                         opt.c_str(), game.c_str(), &stockPilotBind);
    check(stockPilotOk && stockPilotBind.count == 1 && stockPilotBind.slot[0].binding.vk == VK_F4 &&
              std::strcmp(stockPilotBind.file, "ControlSchemes\\Pilot.binds") == 0,
          "#182: absent Pilot user binds falls through to stock ControlSchemes\\Pilot.binds instead of Pilot.SRV.4.2.binds");
    DeleteFileW((schemes + L"\\Pilot.binds").c_str());
    DeleteFileW((opt + L"\\Pilot.SRV.4.2.binds").c_str());
    DeleteFileW((opt + L"\\Pilot.backup.binds").c_str());

    // Unsafe preset name with path traversal is rejected from ControlSchemes.
    writeFile(game + L"\\escape.binds", binds("\t<HMDReset>\n\t\t<Primary Device=\"Keyboard\" Key=\"Key_F8\" />\n\t</HMDReset>\n"));
    writeFile(opt + L"\\StartPreset.4.start", "..\\escape\r\nStockShip\r\nSrvPreset\r\nStockFoot\r\n");
    check(!eliteBindsActionContextDir("HMDReset", EliteBindContext::General, opt.c_str(), game.c_str(), &hmd),
          "#182: unsafe preset name with path traversal is rejected from ControlSchemes");

    DeleteFileW((game + L"\\escape.binds").c_str());
    DeleteFileW((schemes + L"\\StockGen.binds").c_str());
    DeleteFileW((schemes + L"\\StockShip.binds").c_str());
    DeleteFileW((schemes + L"\\StockFoot.binds").c_str());
    RemoveDirectoryW(schemes.c_str());
    RemoveDirectoryW(game.c_str());
    DeleteFileW((opt + L"\\StartPreset.4.start").c_str());
    DeleteFileW((opt + L"\\GenPreset.4.2.binds").c_str());
    DeleteFileW((opt + L"\\ShipPreset.4.2.binds").c_str());
    DeleteFileW((opt + L"\\SrvPreset.4.2.binds").c_str());
    RemoveDirectoryW(opt.c_str());
    RemoveDirectoryW(root.c_str());
    check(GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES, "the #182 scratch folder is gone");
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && !wcscmp(argv[1], L"--dry-run")) {
        std::puts("seat_press_test: dry-run (no files, no devices)");
        return 0;
    }
    if (argc != 2 || wcscmp(argv[1], L"--self-test")) {
        std::fputs("usage: --self-test | --dry-run\n", stderr);
        return 2;
    }
    seansBind();
    keyboardAndHotas();
    unobservable();
    mouse();
    absentAndNeighbours();
    turretKey();
    folder();
    srvContext();
    perContextPresets();
    std::printf("seat_press_test: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
