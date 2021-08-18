
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

typedef JNIIMPORT jint (JNICALL* Func_CreateJavaVM)(JavaVM** pvm, void** env, void* args);


constexpr auto MAX_LOADSTRING = 100;
constexpr auto JVM_OPTION_NUM = 4;

#define DEBUG

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

#ifdef DEBUG
void CreateConsole();
#endif
jobjectArray makeJavaMainArgs(JNIEnv* jniEnv);
std::string findJvmDllLocation(std::string currentDir);
std::vector<std::string> traversalLibs(std::string libDir);
JNIEnv* makeArgsAndCreateJavaVM(std::vector<std::string> libList, std::string jvmLocation);

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
    std::vector<std::string> libList = traversalLibs(libDir);

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
    std::string mainClassName = "Test/Main";
    jclass mainClass = jniEnv->FindClass(mainClassName.c_str());
    if (mainClass == nullptr) 
    {
        jniEnv->ExceptionDescribe();
        javaVM->DestroyJavaVM();
        std::cout << "Could not find the main class" << std::endl;
        return 2;
    }

    //메인 메소드 탐색
    jmethodID mainMethod = jniEnv->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    
    if(mainMethod == nullptr)
    {
        std::cout << "Could not find the main method." << std::endl;
        return 3;
    }
    //메인 메소드 실행
    jobjectArray args = makeJavaMainArgs(jniEnv);
	jniEnv->CallStaticVoidMethod(mainClass, mainMethod, args);
    
    if(jniEnv->ExceptionOccurred())
    {
        std::cout << "Error occurred while invoking the main method..." << std::endl;
    }

    std::cout << "main method is called" << std::endl;

    //while (true) { Sleep(1000); };


    WaitForSingleObject(javaVM, INFINITE);

    return 0;
}

#ifdef DEBUG
void CreateConsole()
{
    // i borrowed this function from https://stackoverflow.com/questions/57210117/unable-to-write-to-allocconsole

    if (!AllocConsole()) {
        return;
    }

    std::cout << "Console is allocated" << std::endl;

    // std::cout, std::clog, std::cerr, std::cin
    FILE* fDummy;
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

    LPWSTR argWstr = GetCommandLineW();
    std::wcout << L"command Line argument:" << argWstr << std::endl;

    jclass stringClass = jniEnv->FindClass("java/lang/String");
    jobjectArray args = nullptr;

    if(wcslen(argWstr) > 0)
    {
        int argc = 0;
        const LPWSTR* argvArray = CommandLineToArgvW(argWstr, &argc);
        std::cout << "argc: " << argc << std::endl;
        if (argc > 1)
        {
            //자바 String클래스 정의 및 args 배열 생성.
            jclass stringClass = jniEnv->FindClass("java/lang/String");
            args = jniEnv->NewObjectArray(argc - 1, stringClass, NULL);

            for (int i = 1; i < argc; i++)
            {
                std::wcout << L"argv[" << i << L"]: " << argvArray[i] << std::endl;

                const size_t argLen = wcslen(argvArray[i]);
                jstring jArg = jniEnv->NewString(reinterpret_cast<jchar*>(argvArray[i]), argLen);

                /*WideCharToMultiByte(CP_UTF8, 0, argvArray[i], argLen, buf, MAX_PATH, NULL, NULL);
                jstring jArg = jniEnv->NewStringUTF(buf);*/
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

std::string findJvmDllLocation(std::string currentDir) {

    BOOL found = FALSE;
	const std::vector<std::string> candidates = { "jre8\\bin\\server\\jvm.dll", "jre8\\bin\\client\\jvm.dll",
        "jdk6\\bin\\client\\jvm.dll" };

    std::filesystem::path currentDirPath = std::filesystem::path(currentDir);
    std::filesystem::path jvmDllLocation;

    
    for (unsigned int i = 0; i < candidates.size(); i++)
    {
        std::filesystem::path tmpLocation = currentDirPath / candidates[i];
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
        std::string retString = jvmDllLocation.string();
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
    for(unsigned int i=0; i < libList.size(); i++)
    {
        //combinedJarPaths += "\"";
    	combinedJarPaths += ";";
        combinedJarPaths += libList[i];
        //combinedJarPaths += "\"";
    }
    
    options[3].optionString = const_cast<char*>(combinedJarPaths.c_str());



    //JVM DLL 로드
    HMODULE dllModule = LoadLibraryA(jvmLocation.c_str());
    if (dllModule == nullptr)
    {
        std::cout << "Error loading jvm.dll. Exit...n" << std::endl;
        return nullptr;
    }
    Func_CreateJavaVM createJavaVm = (Func_CreateJavaVM)(GetProcAddress(dllModule, "JNI_CreateJavaVM"));

    JNIEnv* jniEnv = nullptr;
    jint ret = createJavaVm(&javaVM, (void**)&jniEnv, &vmArgs);

    if (ret == JNI_ERR) {
        std::cout << "Error creating VM. Exit...n" << std::endl;
        return nullptr;
    }
    return jniEnv;
}

// int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
//                      _In_opt_ HINSTANCE hPrevInstance,
//                      _In_ LPWSTR    lpCmdLine,
//                      _In_ int       nCmdShow)
// {
//     UNREFERENCED_PARAMETER(hPrevInstance);
//     UNREFERENCED_PARAMETER(lpCmdLine);
//
//     // TODO: 여기에 코드를 입력합니다.
//
//     // 전역 문자열을 초기화합니다.
//     LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
//     LoadStringW(hInstance, IDC_NATIVEJNILAUNCHER, szWindowClass, MAX_LOADSTRING);
//     MyRegisterClass(hInstance);
//
//     // 애플리케이션 초기화를 수행합니다:
//     if (!InitInstance (hInstance, nCmdShow))
//     {
//         return FALSE;
//     }
//
//     HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NATIVEJNILAUNCHER));
//
//     MSG msg;
//
//     // 기본 메시지 루프입니다:
//     while (GetMessage(&msg, nullptr, 0, 0))
//     {
//         if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
//         {
//             TranslateMessage(&msg);
//             DispatchMessage(&msg);
//         }
//     }
//
//     return (int) msg.wParam;
// }



////
////  함수: MyRegisterClass()
////
////  용도: 창 클래스를 등록합니다.
////
//ATOM MyRegisterClass(HINSTANCE hInstance)
//{
//    WNDCLASSEXW wcex;
//
//    wcex.cbSize = sizeof(WNDCLASSEX);
//
//    wcex.style          = CS_HREDRAW | CS_VREDRAW;
//    wcex.lpfnWndProc    = WndProc;
//    wcex.cbClsExtra     = 0;
//    wcex.cbWndExtra     = 0;
//    wcex.hInstance      = hInstance;
//    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NATIVEJNILAUNCHER));
//    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
//    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
//    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NATIVEJNILAUNCHER);
//    wcex.lpszClassName  = szWindowClass;
//    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
//
//    return RegisterClassExW(&wcex);
//}
//
////
////   함수: InitInstance(HINSTANCE, int)
////
////   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
////
////   주석:
////
////        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
////        주 프로그램 창을 만든 다음 표시합니다.
////
//BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
//{
//   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.
//
//   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
//      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
//
//   if (!hWnd)
//   {
//      return FALSE;
//   }
//
//   ShowWindow(hWnd, nCmdShow);
//   UpdateWindow(hWnd);
//
//   return TRUE;
//}
//
////
////  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
////
////  용도: 주 창의 메시지를 처리합니다.
////
////  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
////  WM_PAINT    - 주 창을 그립니다.
////  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
////
////
//LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//    switch (message)
//    {
//    case WM_COMMAND:
//        {
//            int wmId = LOWORD(wParam);
//            // 메뉴 선택을 구문 분석합니다:
//            switch (wmId)
//            {
//            case IDM_ABOUT:
//                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
//                break;
//            case IDM_EXIT:
//                DestroyWindow(hWnd);
//                break;
//            default:
//                return DefWindowProc(hWnd, message, wParam, lParam);
//            }
//        }
//        break;
//    case WM_PAINT:
//        {
//            PAINTSTRUCT ps;
//            HDC hdc = BeginPaint(hWnd, &ps);
//            // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
//            EndPaint(hWnd, &ps);
//        }
//        break;
//    case WM_DESTROY:
//        PostQuitMessage(0);
//        break;
//    default:
//        return DefWindowProc(hWnd, message, wParam, lParam);
//    }
//    return 0;
//}
//
//// 정보 대화 상자의 메시지 처리기입니다.
//INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
//{
//    UNREFERENCED_PARAMETER(lParam);
//    switch (message)
//    {
//    case WM_INITDIALOG:
//        return (INT_PTR)TRUE;
//
//    case WM_COMMAND:
//        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
//        {
//            EndDialog(hDlg, LOWORD(wParam));
//            return (INT_PTR)TRUE;
//        }
//        break;
//    }
//    return (INT_PTR)FALSE;
//}
