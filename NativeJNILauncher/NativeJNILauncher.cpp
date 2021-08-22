
// NativeJNILauncher.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//


#include "framework.h"
#include "NativeJNILauncher.h"
#include <jni.h>
#include <string>
#include <vector>
#include <iostream>
#include <Shlwapi.h>
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
std::string findJvmDllLocation(std::string currentDir);
std::vector<std::string> traversalLibs(std::string libDir);
JNIEnv* makeArgsAndCreateJavaVM(std::vector<std::string> libList, std::string jvmLocation);
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
#endif

    //현재 파일 위치.
    const LPSTR currentDirApi = new CHAR[_MAX_PATH];
	GetCurrentDirectoryA(_MAX_PATH, currentDirApi);
    const std::string currentDir = currentDirApi;

#ifdef DEBUG
    std::cout << "CurrentDir: " << currentDir << std::endl;
#endif

    //jvm.dll 위치.
    const std::string jvmLocation = findJvmDllLocation(currentDir);
    if (jvmLocation.length() == 0) return -1;

#ifdef DEBUG
    std::cout << "JVM Location: " << jvmLocation << std::endl;
#endif


    //ClassPath(lib/*.jar) 목록 생성
    const std::string libDir = currentDir + "/lib";
    const std::vector<std::string> libList = traversalLibs(libDir);

#ifdef DEBUG
    std::cout << "libDir: " << libDir << std::endl;
    std::cout << "libList: " << std::endl;
    for (std::string lib : libList)
    {
        std::cout << lib << std::endl;
    }
#endif

    //Java VM Args 생성 및 JVM 인스턴스 생성
    JNIEnv* jniEnv = makeArgsAndCreateJavaVM(libList, jvmLocation);

    if(jniEnv == nullptr)
    {
        std::cout << "Starting JVM Instance is failed." << std::endl;
        return 3;
    }
    

    //메인클래스 탐색
    const std::string mainClassName = "Test/Main";
    const jclass mainClass = jniEnv->FindClass(mainClassName.c_str());
    if (mainClass == nullptr) 
    {
        jniEnv->ExceptionDescribe();
        javaVM->DestroyJavaVM();
        std::cout << "Could not find the main class" << std::endl;
        return 2;
    }

    //메인 메소드 탐색
    const jmethodID mainMethod = jniEnv->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    
    if(mainMethod == nullptr)
    {
        std::cout << "Could not find the main method." << std::endl;
        return 3;
    }
    //메인 메소드 실행
    const jobjectArray args = makeJavaMainArgs(jniEnv);
	jniEnv->CallStaticVoidMethod(mainClass, mainMethod, args);
    
    if(jniEnv->ExceptionOccurred())
    {
        std::cout << "Error occurred while invoking the main method..." << std::endl;
    }

#ifdef DEBUG
    std::cout << "main method is called" << std::endl;
#endif

    //while (true) { Sleep(1000); };


    WaitForSingleObject(javaVM, INFINITE);

    return 0;
}

#ifdef DEBUG
void CreateConsole()
{
    // I borrowed this function from https://stackoverflow.com/questions/57210117/unable-to-write-to-allocconsole

    if (!AllocConsole()) {
        return;
    }

    std::cout << "Console is allocated" << std::endl;

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
    HANDLE hConOut = CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hConIn = CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
    //Windows 제공 argument 파서는 Wide char 기준으로 제공됨.

    const LPWSTR argWstr = GetCommandLineW();
#ifdef DEBUG
    std::wcout << L"command Line argument:" << argWstr << std::endl;
#endif

    jclass stringClass = jniEnv->FindClass("java/lang/String");
    jobjectArray args = nullptr;

    if(wcslen(argWstr) > 0)
    {
        int argc = 0;
        const LPWSTR* argvArray = CommandLineToArgvW(argWstr, &argc);

#ifdef DEBUG
        std::cout << "argc: " << argc << std::endl;
#endif

        if (argc > 1)
        {
            //자바 String클래스 정의 및 args 배열 생성.
            jclass stringClass = jniEnv->FindClass("java/lang/String");
            args = jniEnv->NewObjectArray(argc - 1, stringClass, NULL);

            for (int i = 1; i < argc; i++)
            {
#ifdef DEBUG
                std::wcout << L"argv[" << i << L"]: " << argvArray[i] << std::endl;
#endif
                const size_t argLen = wcslen(argvArray[i]);
                jstring jArg = jniEnv->NewString(reinterpret_cast<jchar*>(argvArray[i]), argLen);

                jniEnv->SetObjectArrayElement(args, i - 1, jArg);
            }
        }
    }

    if(args == nullptr)
    {
        args = jniEnv->NewObjectArray(0, stringClass, NULL);
    }

    return args;    
}

std::string findJvmDllLocation(const std::string currentDir) {

    BOOL found = FALSE;
	const std::vector<std::string> candidates = { R"(jre8\bin\server\jvm.dll)", R"(jre8\bin\client\jvm.dll)",
        R"(jdk6\bin\client\jvm.dll)" };

    const std::filesystem::path currentDirPath = std::filesystem::path(currentDir);
    std::filesystem::path jvmDllLocation;

    
    for (const auto& candidate : candidates)
    {
        std::filesystem::path tmpLocation = currentDirPath / candidate;
        if(std::filesystem::exists(tmpLocation))
        {
            //존재하면 바로 리턴.
            found = TRUE;
            jvmDllLocation = tmpLocation;
            break;
        }
        else
        {
            //존재하지 않으면 다음 후보로 이동.
            continue;
        }
    }

    if(found)
    {
        const std::string retString = jvmDllLocation.string();
        return retString;
    }
    else
    {
        return "";
    }

}

std::vector<std::string> traversalLibs(std::string libDir)
{
    std::vector<std::string> listLibs;

    std::filesystem::path libDirPath = std::filesystem::path(libDir);
    std::filesystem::path libDirRelativePath = "." / libDirPath.filename();
    if(std::filesystem::exists(libDirPath))

    for (const auto& file : std::filesystem::recursive_directory_iterator(libDirPath))
    {
        std::filesystem::path filePath = file.path();
        if (!file.is_directory() && filePath.extension() == ".jar")
        {
            std::filesystem::path relativePath = libDirRelativePath / filePath.filename();
            listLibs.push_back(relativePath.string());
        }
    }
    return listLibs;
}

JNIEnv* makeArgsAndCreateJavaVM(std::vector<std::string> libList, std::string jvmLocation)
{
    JavaVMInitArgs vmArgs;
    JavaVMOption options[JVM_OPTION_NUM];

    vmArgs.version = JNI_VERSION_1_6;
    vmArgs.nOptions = JVM_OPTION_NUM;
    vmArgs.ignoreUnrecognized = TRUE;
    vmArgs.options = options;
    
    options[0].optionString = const_cast<char*>("-Xms512M");
    options[1].optionString = const_cast<char*>("-Xmx768M");
    options[2].optionString = const_cast<char*>("-Dfile.encoding=UTF-8");

    std::string combinedJarPaths = "-Djava.class.path=.;";
    for (auto& i : libList)
    {
        combinedJarPaths += ";" + i;
    }
    
    options[3].optionString = const_cast<char*>(combinedJarPaths.c_str());



    //JVM DLL 로드
    HMODULE dllModule = LoadLibraryA(jvmLocation.c_str());
    if (dllModule == nullptr)
    {
        std::cout << "Error loading jvm.dll. Exit...n" << std::endl;
        return nullptr;
    }
    const Func_CreateJavaVM createJavaVm = reinterpret_cast<Func_CreateJavaVM>(GetProcAddress(dllModule, "JNI_CreateJavaVM"));

    JNIEnv* jniEnv = nullptr;
    jint ret = createJavaVm(&javaVM, &jniEnv, &vmArgs);

    if (ret == JNI_ERR) {
        std::cout << "Error creating VM. Exit...n" << std::endl;
        return nullptr;
    }
    return jniEnv;
}