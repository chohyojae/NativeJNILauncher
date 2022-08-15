
// NativeJNILauncher.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//


#include "framework.h"
#include "NativeJNILauncher.h"
#include <jni.h>
#include <string>
#include <vector>
#include <iostream>
#include <shellapi.h>
#include <filesystem>

typedef JNIIMPORT jint (JNICALL* Func_CreateJavaVM)(JavaVM** pvm, JNIEnv** env, void* args);

constexpr auto MAX_LOADSTRING = 100;
constexpr auto JVM_OPTION_NUM = 4;

#define DEBUG

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
jobjectArray makeJavaMainArgs(JNIEnv* jniEnv);
LPWSTR findJvmDllLocation(LPCWSTR currentDir);
std::vector<LPWSTR> traversalLibs(LPCWSTR libDir);
JNIEnv* makeArgsAndCreateJavaVM(std::vector<LPWSTR> libList, LPWSTR jvmLocation);
#ifdef DEBUG
void CreateConsole();
#endif

JavaVM* javaVM = nullptr;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    
#ifdef DEBUG
    CreateConsole();
    std::cout << "Start" << std::endl;
    Sleep(100);
#endif

    //현재 파일 위치.
    LPWSTR currentDir = new WCHAR[_MAX_PATH] {0, };
	GetCurrentDirectoryW(_MAX_PATH, currentDir);

#ifdef DEBUG
    std::wcout << L"CurrentDir: " << currentDir << std::endl;
    Sleep(100);
#endif

    //jvm.dll 위치.
    LPWSTR jvmLocation = findJvmDllLocation(currentDir);
#ifdef DEBUG
    std::cout << "jvmLocation Length : " << wcslen(jvmLocation) << std::endl;
#endif
    if (wcslen(jvmLocation) == 0) {
        return -1;
    }

#ifdef DEBUG
    std::cout << "jvmLocation Length : " << wcslen(jvmLocation) << std::endl;
    std::wcout << L"JVM Location: " << jvmLocation << std::endl;
    Sleep(1000);
#endif

    //ClassPath(lib/*.jar) 목록 생성
    LPWSTR libDir = new WCHAR[_MAX_PATH] {0, };
    wcscat_s(libDir, _MAX_PATH, currentDir);
    wcscat_s(libDir, _MAX_PATH, L"\\lib");
    std::vector<LPWSTR> libList = traversalLibs(libDir);

#ifdef DEBUG
    std::wcout << L"libDir: " << libDir << std::endl;
    std::wcout << L"libList: " << std::endl;
    for (const LPCWSTR& lib : libList)
    {
        std::wcout << lib << std::endl;
    }

    Sleep(1000);
#endif


    //Java VM Args 생성 및 JVM 인스턴스 생성
    JNIEnv* jniEnv = makeArgsAndCreateJavaVM(libList, jvmLocation);

    // 필요 없어진 currentDir, libDir 및 libList free
    delete[] currentDir;
    delete[] libDir;
    for (LPWSTR i : libList)
    {
        free(i);
    }
    libList.clear();
    libList.shrink_to_fit();

    if(jniEnv == nullptr)
    {
        std::cout << "Starting JVM Instance is failed." << std::endl;

#ifdef DEBUG
        Sleep(100);
#endif
        return 3;
    }
    
    // 메인클래스 탐색
    const std::string mainClassName = "com/menutest/Main";
    const jclass mainClass = jniEnv->FindClass(mainClassName.c_str());
    if (mainClass == nullptr) 
    {
        jniEnv->ExceptionDescribe();
        javaVM->DestroyJavaVM();
        std::cout << "Could not find the main class" << std::endl;
#ifdef DEBUG
        Sleep(100);
#endif
        return 2;
    }

    // 메인 메소드 탐색
    const jmethodID mainMethod = jniEnv->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    
    if(mainMethod == nullptr)
    {
        std::cout << "Could not find the main method." << std::endl;
        return 3;
    }

     
    // 메인 메소드 실행
    const jobjectArray args = makeJavaMainArgs(jniEnv);
	jniEnv->CallStaticVoidMethod(mainClass, mainMethod, args);
    jniEnv->DeleteLocalRef(mainClass);
    jniEnv->DeleteLocalRef(args);

    if(jniEnv->ExceptionOccurred())
    {
        std::cout << "Exception occurred while invoking the main method..." << std::endl;
#ifdef DEBUG
        Sleep(100);
#endif
    }

#ifdef DEBUG
    std::cout << "main method is called" << std::endl;
    Sleep(100);
#endif

#ifdef DEBUG
    std::cout << "Invoke Destroy VM" << std::endl;
#endif
    javaVM->DestroyJavaVM();

    return 0;
}

#ifdef DEBUG

void CreateConsole()
{
    // I borrowed this function from https://stackoverflow.com/questions/57210117/unable-to-write-to-allocconsole

    if (!AttachConsole(-1)) {
        return;
    }

    // std::cout, std::clog, std::cerr, std::cin
    FILE* fDummy = nullptr;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    // std::wcout, std::wclog, std::wcerr, std::wcin
    const HANDLE hConOut = CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    const HANDLE hConIn = CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
    SetStdHandle(STD_ERROR_HANDLE, hConOut);
    SetStdHandle(STD_INPUT_HANDLE, hConIn);
    std::wcout.clear();
    std::wclog.clear();
    std::wcerr.clear();
    std::wcin.clear();
}
#endif

jobjectArray makeJavaMainArgs(JNIEnv* jniEnv)
{
    const LPWSTR argWstr = GetCommandLineW();
#ifdef DEBUG
    std::wcout << L"command Line argument:" << argWstr << std::endl;
#endif

    const jclass stringClass = jniEnv->FindClass("java/lang/String");
    jobjectArray args = nullptr;

    if(wcslen(argWstr) > 0)
    {
        int argc = 0;
        LPWSTR* argvArray = CommandLineToArgvW(argWstr, &argc);

#ifdef DEBUG
        std::cout << "argc: " << argc << std::endl;
#endif

        if (argc > 1)
        {
            //자바 args 배열 생성.
            args = jniEnv->NewObjectArray(argc - 1, stringClass, NULL);

            for (int i = 1; i < argc; i++)
            {
#ifdef DEBUG
                std::wcout << L"argv[" << i << L"]: " << argvArray[i] << std::endl;
#endif
                const size_t argLen = wcslen(argvArray[i]);
                // char buffer[1024] = {0,};
                // WideCharToMultiByte(CP_ACP, 0, argvArray[i], lstrlenW(argvArray[i]), buffer, 1024, 0, 0);
                const jstring jArg = jniEnv->NewString(reinterpret_cast<jchar*>(argvArray[i]), argLen);

                jniEnv->SetObjectArrayElement(args, i - 1, jArg);
                jniEnv->DeleteLocalRef(jArg);
            }
        }
        LocalFree(argvArray);
    }

    if(args == nullptr)
    {
        args = jniEnv->NewObjectArray(0, stringClass, NULL);
    }
    jniEnv->DeleteLocalRef(stringClass);
    return args;    
}

LPWSTR findJvmDllLocation(LPCWSTR currentDir) {

    BOOL found = FALSE;
	const std::vector<LPCWSTR> candidates = { LR"(jre8\bin\server\jvm.dll)", LR"(jre8\bin\client\jvm.dll)",
        LR"(jdk6\bin\client\jvm.dll)" };

    const std::filesystem::path currentDirPath = std::filesystem::path(currentDir);
    std::filesystem::path jvmDllLocation;
    
    for (const auto& candidate : candidates)
    {
        std::filesystem::path tmpLocation = currentDirPath / candidate;
        if (std::filesystem::exists(tmpLocation))
        {
            //존재하면 바로 리턴.
            found = TRUE;
            jvmDllLocation = tmpLocation;
            break;
        }
        //존재하지 않으면 다음 후보로 이동.
        // else continue;
    }

    if (!found) {
        return _wcsdup(L"");
    }

    LPWSTR retString = _wcsdup(jvmDllLocation.wstring().c_str());
#ifdef DEBUG
    std::wcout << "jvmDllLocation: " << jvmDllLocation << std::endl;
    std::wcout << "retString: " << retString << std::endl;
#endif
    return retString;
}

std::vector<LPWSTR> traversalLibs(LPCWSTR libDir)
{
    std::vector<LPWSTR> listLibs;

    const std::filesystem::path libDirPath = std::filesystem::path(libDir);
    const std::filesystem::path libDirRelativePath = "." / libDirPath.filename();
    if (std::filesystem::exists(libDirPath)) {
        for (const auto& file : std::filesystem::recursive_directory_iterator(libDirPath))
        {
            std::filesystem::path filePath = file.path();
            if (!file.is_directory() && filePath.extension() == L".jar")
            {
                std::filesystem::path relativePath = libDirRelativePath / filePath.filename();
                listLibs.push_back(_wcsdup(relativePath.wstring().c_str()));
            }
        }
    }
    return listLibs;
}

JNIEnv* makeArgsAndCreateJavaVM(std::vector<LPWSTR> libList, LPWSTR jvmLocation)
{
    JavaVMInitArgs vmArgs {};
    JavaVMOption options[JVM_OPTION_NUM] {};

    vmArgs.version = JNI_VERSION_1_6;
    vmArgs.nOptions = JVM_OPTION_NUM;
    vmArgs.ignoreUnrecognized = TRUE;
    vmArgs.options = options;
    
    options[0].optionString = _strdup("-Xms512M");
    options[1].optionString = _strdup("-Xmx768M");
    options[2].optionString = _strdup("-Dfile.encoding=UTF-8");

    std::wstring combinedJarPaths = L"-Djava.class.path=.;";
    for (LPWSTR i : libList)
    {
        combinedJarPaths += i;
        combinedJarPaths += L";";
    }
    char buffer[1024] = { 0, };
    LPCWSTR wPaths = combinedJarPaths.c_str();
    WideCharToMultiByte(CP_ACP, 0, wPaths, lstrlenW(wPaths), buffer, 1024, 0, 0);
    options[3].optionString = _strdup(buffer);

#ifdef DEBUG
    std::cout << "JVM Options: " << std::endl;
    for (int i=0; i<JVM_OPTION_NUM; i++)
    {
        std::cout << "#" << i << " " << options[i].optionString << std::endl;
    }
#endif

    //JVM DLL 로드
    const HMODULE dllModule = LoadLibraryW(jvmLocation);
    if (dllModule == nullptr)
    {
        std::cout << "Error loading jvm.dll. Exit...n" << std::endl;
        return nullptr;
    }
    const Func_CreateJavaVM createJavaVm = reinterpret_cast<Func_CreateJavaVM>(GetProcAddress(dllModule, "JNI_CreateJavaVM"));

    JNIEnv* jniEnv = nullptr;
    const jint ret = createJavaVm(&javaVM, &jniEnv, &vmArgs);

    free(options[0].optionString);
    free(options[1].optionString);
    free(options[2].optionString);
    free(options[3].optionString);

    if (ret == JNI_ERR) {
        std::cout << "Error creating VM. Exit...n" << std::endl;
        return nullptr;
    }
    return jniEnv;
}