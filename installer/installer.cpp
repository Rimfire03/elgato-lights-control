// Installateur autonome du plugin OBS "Elgato Lights Control".
// La DLL du plugin est embarquee dans l'executable (ressource RCDATA 101).
//
//   Install-Elgato-Lights.exe              installation interactive
//   Install-Elgato-Lights.exe /S           installation silencieuse
//   Install-Elgato-Lights.exe /uninstall   desinstallation (ajouter /S pour le silencieux)
//
// Destination : %ProgramData%\obs-studio\plugins\obs-elgato-lights\bin\64bit\ (dossier lu par OBS 28+).

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

#ifndef APP_VERSION_W
#define APP_VERSION_W L"1.0.0"
#endif

static const wchar_t *kTitle = L"Elgato Lights Control - OBS";
static const wchar_t *kPluginName = L"obs-elgato-lights";
static const int kResId = 101;

static bool g_silent = false;

static int ask(const std::wstring &text, UINT flags)
{
	if (g_silent)
		return IDYES;
	return MessageBoxW(nullptr, text.c_str(), kTitle, flags);
}

static void info(const std::wstring &text, bool error = false)
{
	if (!g_silent)
		MessageBoxW(nullptr, text.c_str(), kTitle, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

static bool obsRunning()
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return false;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);
	bool found = false;
	if (Process32FirstW(snap, &pe)) {
		do {
			if (_wcsicmp(pe.szExeFile, L"obs64.exe") == 0) {
				found = true;
				break;
			}
		} while (Process32NextW(snap, &pe));
	}
	CloseHandle(snap);
	return found;
}

// Dossier d'installation d'OBS (registre), vide si introuvable.
static std::wstring obsDir()
{
	const REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
	for (REGSAM v : views) {
		HKEY key;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio", 0, KEY_READ | v, &key) ==
		    ERROR_SUCCESS) {
			wchar_t buf[MAX_PATH]{};
			DWORD size = sizeof(buf);
			LONG r = RegQueryValueExW(key, nullptr, nullptr, nullptr, (LPBYTE)buf, &size);
			RegCloseKey(key);
			if (r == ERROR_SUCCESS && buf[0]) {
				std::wstring exe = std::wstring(buf) + L"\\bin\\64bit\\obs64.exe";
				if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES)
					return buf;
			}
		}
	}
	return std::wstring();
}

static bool obsInstalled()
{
	return !obsDir().empty();
}

// Relance OBS apres une mise a jour automatique (exe, repertoire de travail = bin\64bit).
static void relaunchObs()
{
	const std::wstring dir = obsDir();
	if (dir.empty())
		return;
	const std::wstring wd = dir + L"\\bin\\64bit";
	std::wstring cmd = L"\"" + wd + L"\\obs64.exe\"";
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, wd.c_str(), &si,
			   &pi)) {
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

static std::wstring pluginRoot()
{
	PWSTR pd = nullptr;
	std::wstring path;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &pd)) && pd)
		path = pd;
	CoTaskMemFree(pd);
	return path + L"\\obs-studio\\plugins\\" + kPluginName;
}

// ---- Entree "Ajout/Suppression de programmes" (HKCU : pas de droits admin necessaires) ----

static const wchar_t *kUninstallKey =
	L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ObsElgatoLightsControl";
static const wchar_t *kUninstallExe = L"Uninstall-Elgato-Lights.exe";

static void setStr(HKEY k, const wchar_t *name, const std::wstring &v)
{
	RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
}

static void setDword(HKEY k, const wchar_t *name, DWORD v)
{
	RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&v, sizeof(v));
}

static void registerUninstall(const std::wstring &root, const std::wstring &uninstaller, DWORD sizeBytes)
{
	HKEY k;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) !=
	    ERROR_SUCCESS)
		return;
	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t date[16];
	wsprintfW(date, L"%04u%02u%02u", st.wYear, st.wMonth, st.wDay);

	setStr(k, L"DisplayName", L"Elgato Lights Control (plugin OBS)");
	setStr(k, L"DisplayVersion", APP_VERSION_W);
	setStr(k, L"Publisher", L"TomLine prod&co");
	setStr(k, L"InstallLocation", root);
	setStr(k, L"InstallDate", date);
	setStr(k, L"DisplayIcon", uninstaller);
	setStr(k, L"UninstallString", L"\"" + uninstaller + L"\" /uninstall");
	setStr(k, L"QuietUninstallString", L"\"" + uninstaller + L"\" /uninstall /S");
	setDword(k, L"NoModify", 1);
	setDword(k, L"NoRepair", 1);
	setDword(k, L"EstimatedSize", sizeBytes / 1024 + 1);
	RegCloseKey(k);
}

static void unregisterUninstall()
{
	RegDeleteKeyW(HKEY_CURRENT_USER, kUninstallKey);
}

static bool installPlugin(std::wstring &err)
{
	HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(kResId), RT_RCDATA);
	HGLOBAL h = res ? LoadResource(nullptr, res) : nullptr;
	const void *data = h ? LockResource(h) : nullptr;
	const DWORD size = res ? SizeofResource(nullptr, res) : 0;
	if (!data || !size) {
		err = L"Le plugin n'est pas embarque dans l'installateur.";
		return false;
	}

	const std::wstring dir = pluginRoot() + L"\\bin\\64bit";
	const int rc = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
	if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS && rc != ERROR_FILE_EXISTS) {
		err = L"Impossible de creer le dossier :\n" + dir;
		return false;
	}

	const std::wstring dll = dir + L"\\" + kPluginName + L".dll";
	const std::wstring tmp = dll + L".new";
	HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE) {
		err = L"Impossible d'ecrire dans :\n" + dir + L"\n\nRelancez l'installateur en tant qu'administrateur.";
		return false;
	}
	DWORD written = 0;
	const BOOL ok = WriteFile(f, data, size, &written, nullptr);
	CloseHandle(f);
	if (!ok || written != size) {
		DeleteFileW(tmp.c_str());
		err = L"Ecriture du plugin incomplete.";
		return false;
	}
	if (!MoveFileExW(tmp.c_str(), dll.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(tmp.c_str());
		err = L"Impossible de remplacer le plugin existant (OBS est-il ferme ?).";
		return false;
	}

	// Copie de l'installateur : il sert de desinstallateur (entree Ajout/Suppression de programmes).
	const std::wstring root = pluginRoot();
	const std::wstring uninstaller = root + L"\\" + kUninstallExe;
	wchar_t self[MAX_PATH]{};
	GetModuleFileNameW(nullptr, self, MAX_PATH);
	if (_wcsicmp(self, uninstaller.c_str()) != 0)
		CopyFileW(self, uninstaller.c_str(), FALSE);
	registerUninstall(root, uninstaller, size);
	return true;
}

static bool uninstallPlugin(std::wstring &err)
{
	const std::wstring root = pluginRoot();
	const std::wstring dll = root + L"\\bin\\64bit\\" + kPluginName + L".dll";
	if (GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(dll.c_str())) {
		err = L"Impossible de supprimer le plugin (OBS est-il ferme ?).";
		return false;
	}
	DeleteFileW((root + L"\\bin\\64bit\\" + kPluginName + L".pdb").c_str());
	RemoveDirectoryW((root + L"\\bin\\64bit").c_str());
	RemoveDirectoryW((root + L"\\bin").c_str());
	unregisterUninstall();

	// Si on tourne depuis la copie installee, elle ne peut pas se supprimer elle-meme :
	// un cmd detache la efface apres la fin du processus, puis retire le dossier.
	const std::wstring uninstaller = root + L"\\" + kUninstallExe;
	wchar_t self[MAX_PATH]{};
	GetModuleFileNameW(nullptr, self, MAX_PATH);
	if (_wcsicmp(self, uninstaller.c_str()) == 0) {
		std::wstring cmd = L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"" + uninstaller +
				   L"\" & rmdir \"" + root + L"\"";
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS,
				   nullptr, nullptr, &si, &pi)) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
	} else {
		DeleteFileW(uninstaller.c_str());
		RemoveDirectoryW(root.c_str());
	}
	return true;
}

static void removeConfig()
{
	PWSTR ad = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &ad)) || !ad)
		return;
	const std::wstring dir = std::wstring(ad) + L"\\obs-studio\\plugin_config\\" + kPluginName;
	CoTaskMemFree(ad);
	DeleteFileW((dir + L"\\config.json").c_str());
	RemoveDirectoryW(dir.c_str());
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	bool uninstall = false;
	bool update = false; // lance par le plugin : attend la fermeture d'OBS, installe, relance OBS
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 1; argv && i < argc; ++i) {
		if (_wcsicmp(argv[i], L"/S") == 0 || _wcsicmp(argv[i], L"/silent") == 0)
			g_silent = true;
		else if (_wcsicmp(argv[i], L"/uninstall") == 0)
			uninstall = true;
		else if (_wcsicmp(argv[i], L"/update") == 0)
			update = true;
	}
	if (argv)
		LocalFree(argv);

	const std::wstring version = APP_VERSION_W;

	if (update && !uninstall) {
		// Attente de la fermeture d'OBS (10 min max), puis pause : OBS met quelques secondes a se
		// terminer et un relancement trop rapide bloque la nouvelle instance.
		for (int i = 0; i < 1200 && obsRunning(); ++i)
			Sleep(500);
		if (obsRunning())
			return 2;
		Sleep(3000);
		std::wstring err;
		if (!installPlugin(err)) {
			info(err, true);
			return 3;
		}
		relaunchObs();
		return 0;
	}

	if (!g_silent) {
		const std::wstring q =
			uninstall ? L"Desinstaller le plugin Elgato Lights Control d'OBS Studio ?"
				  : L"Installer le plugin Elgato Lights Control " + version + L" pour OBS Studio ?";
		if (ask(q, MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 1;
	}

	while (obsRunning()) {
		if (g_silent)
			return 2;
		if (ask(L"OBS Studio est en cours d'execution.\n\nFermez-le completement (attendez que la fenetre "
			L"disparaisse), puis cliquez sur Reessayer.",
			MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY)
			return 2;
		Sleep(500);
	}

	std::wstring err;
	if (uninstall) {
		if (!uninstallPlugin(err)) {
			info(err, true);
			return 3;
		}
		if (!g_silent && ask(L"Supprimer aussi la configuration (lumieres, groupes, automatisations) ?",
				     MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
			removeConfig();
		info(L"Plugin desinstalle.");
		return 0;
	}

	if (!obsInstalled() && !g_silent) {
		if (ask(L"OBS Studio n'a pas ete detecte sur ce PC.\n\nInstaller quand meme le plugin ?",
			MB_YESNO | MB_ICONWARNING) != IDYES)
			return 4;
	}

	if (!installPlugin(err)) {
		info(err, true);
		return 3;
	}
	info(L"Plugin installe.\n\nLancez OBS Studio puis ouvrez le menu Docks > Elgato Lights.");
	return 0;
}
