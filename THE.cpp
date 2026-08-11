#include <Windows.h>
#include <string>
#include <vector>

#define IDC_EDIT_CONTENT 1001
#define IDC_BTN_NEW      1002
#define IDC_BTN_OPEN     1003
#define IDC_BTN_SAVE     1004
#define IDC_CHECK_BINARY 1005

HWND hWnd;
HWND hEdit;
HWND hCheckBinary;
std::wstring currPath;
bool isBinary = false;
std::vector<BYTE> dataBuf;

WNDPROC g_OldEditProc = NULL;

LRESULT CALLBACK HexEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CHAR: {
		if (!isBinary) return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
		WCHAR ch = (WCHAR)wParam;
		if (ch == 0x08 || ch == L' ' || (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F')) {
			return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
		}
		else if (ch >= L'a' && ch <= L'f') {
			SendMessage(hwnd, WM_CHAR, (WPARAM)towupper(ch), lParam);
			return 0;
		}
		return 0;
	}
	case WM_PASTE: {
		if (!isBinary) return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
		if (!OpenClipboard(hwnd)) return 0;
		std::wstring clipWStr;
		HANDLE hClipData = GetClipboardData(CF_UNICODETEXT);
		if (hClipData) {
			const WCHAR* pData = (const WCHAR*)GlobalLock(hClipData);
			if (pData) clipWStr = pData;
			GlobalUnlock(hClipData);
		}
		CloseClipboard();
		if (clipWStr.empty()) return 0;
		int ansiLen = WideCharToMultiByte(CP_ACP, 0, clipWStr.c_str(), -1, NULL, 0, NULL, NULL);
		std::vector<BYTE> ansiBuf(ansiLen, 0);
		WideCharToMultiByte(CP_ACP, 0, clipWStr.c_str(), -1, (char*)ansiBuf.data(), ansiLen, NULL, NULL);
		std::wstring hexResult;
		for (int i = 0; i < ansiLen - 1; i++) {
			WCHAR tmp[8];
			swprintf_s(tmp, L"%02X ", ansiBuf[i]);
			hexResult += tmp;
		}
		if (!hexResult.empty()) hexResult.pop_back();
		SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)hexResult.c_str());
		return 0;
	}
	default:
		return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE: {
		hEdit = CreateWindowEx(
			WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN,
			10, 45, 560, 360,
			hwnd, (HMENU)IDC_EDIT_CONTENT, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		g_OldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)HexEditProc);
		CreateWindow(
			L"BUTTON", L"新建", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 10, 50, 25,
			hwnd, (HMENU)IDC_BTN_NEW, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		CreateWindow(
			L"BUTTON", L"打开", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 70, 10, 50, 25,
			hwnd, (HMENU)IDC_BTN_OPEN, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		CreateWindow(
			L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 10, 50, 25,
			hwnd, (HMENU)IDC_BTN_SAVE, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		hCheckBinary = CreateWindow(
			L"BUTTON", L"二进制", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 190, 10, 75, 25,
			hwnd, (HMENU)IDC_CHECK_BINARY, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		break;
	}
	case WM_COMMAND: {
		int id = LOWORD(wParam);
		switch (id) {
		case IDC_BTN_NEW: {
			SetWindowText(hEdit, L"");
			currPath.clear();
			dataBuf.clear();
			break;
		}
		case IDC_BTN_OPEN: {
			OPENFILENAME ofn = { 0 };
			WCHAR szFile[MAX_PATH] = { 0 };
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = L"所有文件(*.*)\0*.*\0文本文件(*.txt)\0*.txt\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileName(&ofn)) {
				currPath = szFile;
				HANDLE hFile = CreateFile(
					szFile, GENERIC_READ, FILE_SHARE_READ, NULL,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
				);
				if (hFile == INVALID_HANDLE_VALUE) break;
				DWORD fileSize = GetFileSize(hFile, nullptr);
				dataBuf.resize(fileSize);
				DWORD readLen;
				ReadFile(hFile, dataBuf.data(), fileSize, &readLen, nullptr);
				CloseHandle(hFile);
				SetWindowText(hEdit, L"");
				if (isBinary) {
					std::wstring hexStr;
					for (auto b : dataBuf) {
						wchar_t hex[4];
						swprintf_s(hex, L"%02X ", b);
						hexStr += hex;
					}
					SetWindowText(hEdit, hexStr.c_str());
				}
				else {
					std::string str(dataBuf.begin(), dataBuf.end());
					SetWindowTextA(hEdit, str.c_str());
				}
			}
			break;
		}
		case IDC_BTN_SAVE: {
			int textLen = GetWindowTextLength(hEdit);
			std::wstring text(textLen + 1, L'\0');
			GetWindowText(hEdit, text.data(), textLen + 1);
			dataBuf.clear();
			if (isBinary) {
				std::wstring cleanedText;
				for (auto ch : text) {
					if (iswspace(ch)) continue;
					cleanedText += ch;
				}
				for (size_t i = 0; i < cleanedText.size(); i += 2) {
					if (i + 1 >= cleanedText.size()) break;
					std::wstring byteStr = cleanedText.substr(i, 2);
					BYTE byte = (BYTE)std::stoi(byteStr, nullptr, 16);
					dataBuf.push_back(byte);
				}
			}
			else {
				int ansiLen = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
				dataBuf.resize(ansiLen - 1);
				WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, (char*)dataBuf.data(), ansiLen, NULL, NULL);
			}
			if (currPath.empty()) {
				OPENFILENAME ofn = { 0 };
				WCHAR szFile[MAX_PATH] = { 0 };
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrFilter = L"所有文件(*.*)\0*.*\0文本文件(*.txt)\0*.txt\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
				if (!GetSaveFileName(&ofn)) break;
				currPath = szFile;
			}
			HANDLE hFile = CreateFile(
				currPath.c_str(), GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
			);
			if (hFile == INVALID_HANDLE_VALUE) {
				MessageBox(hwnd, L"无法保存文件", L"错误", MB_ICONERROR);
				break;
			}
			DWORD writeLen;
			WriteFile(hFile, dataBuf.data(), dataBuf.size(), &writeLen, nullptr);
			CloseHandle(hFile);
			break;
		}
		case IDC_CHECK_BINARY: {
			isBinary = (SendMessage(hCheckBinary, BM_GETCHECK, 0, 0) == BST_CHECKED);
			SetWindowText(hEdit, L"");
			if (isBinary) {
				std::wstring hexStr;
				for (auto b : dataBuf) {
					wchar_t hex[4];
					swprintf_s(hex, L"%02X ", b);
					hexStr += hex;
				}
				SetWindowText(hEdit, hexStr.c_str());
			}
			else {
				std::string str(dataBuf.begin(), dataBuf.end());
				SetWindowTextA(hEdit, str.c_str());
			}
			SendMessage(hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
			break;
		}
		}
		break;
	}
	case WM_CLOSE: {
		DestroyWindow(hwnd);
		break;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"THE";
	RegisterClass(&wc);
	hWnd = CreateWindowEx(
		0, L"THE", L"THE",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 450,
		NULL, NULL, hInstance, NULL
	);
	ShowWindow(hWnd, nCmdShow);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}