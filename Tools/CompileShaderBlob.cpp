// Offline shader bytecode writer using D3DCompileFromFile (same path as runtime JIT).
// Usage: CompileShaderBlob.exe <output.cso> <input.hlsl> <entry> <profile> [MACRO=VALUE ...]

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <Windows.h>
#include <d3dcompiler.h>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	void PrintErr(const char* msg)
	{
		OutputDebugStringA(msg);
		DWORD w = 0;
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, (DWORD)strlen(msg), &w, nullptr);
	}

	bool WriteWholeFile(const wchar_t* path, const void* data, size_t size)
	{
		HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BYTE* p = static_cast<const BYTE*>(data);
		size_t left = size;
		while (left > 0)
		{
			DWORD chunk = left > 0x7fffffffu ? 0x7fffffffu : (DWORD)left;
			if (!WriteFile(h, p, chunk, &written, nullptr) || written != chunk)
			{
				CloseHandle(h);
				return false;
			}
			p += chunk;
			left -= chunk;
		}
		CloseHandle(h);
		return true;
	}

	std::string NarrowUtf8Arg(const wchar_t* w)
	{
		if (!w)
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string s((size_t)n - 1, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
		return s;
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 5)
	{
		PrintErr("CompileShaderBlob: need args <out.dxbc> <in.hlsl> <entry> <profile> [NAME=value ...]\n");
		return 2;
	}

	const wchar_t* outPath = argv[1];
	const wchar_t* hlslPath = argv[2];
	const std::string entry = NarrowUtf8Arg(argv[3]);
	const std::string profile = NarrowUtf8Arg(argv[4]);

	std::vector<D3D_SHADER_MACRO> macros;
	std::vector<std::string> nameStorage;
	std::vector<std::string> defStorage;
	for (int i = 5; i < argc; ++i)
	{
		std::string pair = NarrowUtf8Arg(argv[i]);
		const size_t eq = pair.find('=');
		if (eq == std::string::npos)
		{
			PrintErr("CompileShaderBlob: bad define (expected NAME=value)\n");
			return 2;
		}
		nameStorage.push_back(pair.substr(0, eq));
		defStorage.push_back(pair.substr(eq + 1));
		D3D_SHADER_MACRO m{};
		m.Name = nameStorage.back().c_str();
		m.Definition = defStorage.back().c_str();
		macros.push_back(m);
	}
	macros.push_back({ nullptr, nullptr });

	const UINT flags = 0; // Match Release runtime (ShaderUtil default optimized).

	ID3DBlob* code = nullptr;
	ID3DBlob* errs = nullptr;
	const HRESULT hr = D3DCompileFromFile(hlslPath, macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, entry.c_str(),
		profile.c_str(), flags, 0, &code, &errs);

	if (errs)
	{
		PrintErr(static_cast<const char*>(errs->GetBufferPointer()));
		errs->Release();
	}

	if (FAILED(hr) || !code)
		return 1;

	const bool ok = WriteWholeFile(outPath, code->GetBufferPointer(), code->GetBufferSize());
	code->Release();
	return ok ? 0 : 1;
}
