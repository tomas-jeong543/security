#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "target_rvas.h"

static bool Launch(const std::wstring& text, const std::wstring& cwd, DWORD flags, PROCESS_INFORMATION& pi)
{
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    std::vector<wchar_t> cmd(text.begin(),text.end()); cmd.push_back(0);
    return CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,flags,nullptr,cwd.c_str(),&si,&pi)!=FALSE;
}
template<class T> static bool Read(HANDLE process, std::uintptr_t address, T& value)
{
    SIZE_T got=0;
    return ReadProcessMemory(process,reinterpret_cast<const void*>(address),&value,sizeof(value),&got) && got==sizeof(value);
}
static bool Entry(HANDLE process,std::uintptr_t address,const char* label)
{
    BYTE bytes[16]{};
    const bool ok=Read(process,address,bytes);
    std::printf("ENTRY %s ",label);
    if(ok) for(BYTE b:bytes) std::printf("%02X ",b);
    else std::printf("UNREADABLE error=%lu",GetLastError());
    std::printf("\n"); std::fflush(stdout);
    return ok && bytes[0]==0xe9;
}

int wmain(int argc,wchar_t** argv)
{
    if(argc!=2) return 99;
    const std::wstring cwd=argv[1];
    PROCESS_INFORMATION game{},injector{};
    if(!Launch(L"\""+cwd+L"\\GameClient.exe\" --smoke-gui",cwd,DEBUG_ONLY_THIS_PROCESS,game)) return 98;
    std::uintptr_t base=0;
    bool armed=false,held=false,selected=false,installed=false,bDetected=false,finished=false;
    DWORD gameExit=STILL_ACTIVE;
    const auto start=GetTickCount64();
    while(GetTickCount64()-start<20000)
    {
        DEBUG_EVENT ev{};
        if(!WaitForDebugEvent(&ev,100)) continue;
        DWORD continuation=DBG_CONTINUE;
        if(ev.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT)
        {
            base=reinterpret_cast<std::uintptr_t>(ev.u.CreateProcessInfo.lpBaseOfImage);
            if(ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            std::printf("TARGET pid=%lu base=%llX command=--smoke-gui\n",game.dwProcessId,(unsigned long long)base);
            Entry(game.hProcess,base+SPAWN_RVA,"spawn_before");
            Entry(game.hProcess,base+CHECKED_RVA,"wrapper_before");
        }
        else if(ev.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile);
        else if(ev.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT && armed && !selected)
        {
            // The loader's initial exception return restored an earlier debug
            // context. Arm again at normal thread creation after user entry.
            CONTEXT ctx{}; ctx.ContextFlags=CONTEXT_DEBUG_REGISTERS;
            if(GetThreadContext(game.hThread,&ctx)) {
                ctx.Dr0=base+CHECKED_RVA; ctx.Dr6=0; ctx.Dr7=1;
                const bool ok=SetThreadContext(game.hThread,&ctx)!=FALSE;
                std::printf("HW_REARM_AFTER_USER_ENTRY event_tid=%lu main_tid=%lu success=%d\n",ev.dwThreadId,game.dwThreadId,ok);
            }
        }
        else if(ev.dwDebugEventCode==EXCEPTION_DEBUG_EVENT)
        {
            const auto code=ev.u.Exception.ExceptionRecord.ExceptionCode;
            if(code==EXCEPTION_BREAKPOINT && !armed)
            {
                CONTEXT ctx{}; ctx.ContextFlags=CONTEXT_FULL|CONTEXT_DEBUG_REGISTERS;
                if(!GetThreadContext(game.hThread,&ctx)) { std::printf("ERROR GetContext %lu\n",GetLastError()); break; }
                ctx.Dr0=base+CHECKED_RVA; ctx.Dr6=0; ctx.Dr7=1;
                if(!SetThreadContext(game.hThread,&ctx)) { std::printf("ERROR SetContext %lu\n",GetLastError()); break; }
                CONTEXT verify{}; verify.ContextFlags=CONTEXT_FULL|CONTEXT_DEBUG_REGISTERS;
                const bool got=GetThreadContext(game.hThread,&verify)!=FALSE;
                std::printf("HW_CONTEXT_READBACK event_tid=%lu main_tid=%lu valid=%d rip=%llX dr0=%llX dr7=%llX\n",ev.dwThreadId,game.dwThreadId,got,
                    (unsigned long long)verify.Rip,(unsigned long long)verify.Dr0,(unsigned long long)verify.Dr7);
                armed=true;
                std::printf("HW_EXEC_BREAKPOINT armed wrapper_rva=%llX no_code_byte_patch=1\n",(unsigned long long)CHECKED_RVA);
            }
            else if(code==EXCEPTION_SINGLE_STEP && ev.dwThreadId==game.dwThreadId)
            {
                CONTEXT ctx{}; ctx.ContextFlags=CONTEXT_FULL|CONTEXT_DEBUG_REGISTERS;
                if(!GetThreadContext(game.hThread,&ctx)) break;
                if(ctx.Rip==base+CHECKED_RVA)
                {
                    std::uint64_t caller=0,source=0; int kind=-1,damage=-1;
                    const bool args=Read(game.hProcess,ctx.Rsp,caller) && Read(game.hProcess,ctx.Rsp+0x28,kind) &&
                        Read(game.hProcess,ctx.Rsp+0x30,damage) && Read(game.hProcess,ctx.Rsp+0x38,source);
                    const auto callerRva=caller-base;
                    const bool bot=args && callerRva>=BOT_BEGIN && callerRva<BOT_END;
                    float position[2],direction[2]; std::memcpy(position,&ctx.R8,8); std::memcpy(direction,&ctx.R9,8);
                    std::printf("CHECKED_CALL tid=%lu caller_rva=%llX bot_caller=%d sourceId=%llu team=%u kind=%d damage=%d position=%.6f,%.6f direction=%.6f,%.6f\n",
                        ev.dwThreadId,(unsigned long long)callerRva,bot,(unsigned long long)source,(unsigned)ctx.Rdx,kind,damage,
                        position[0],position[1],direction[0],direction[1]);
                    ctx.EFlags|=0x10000; ctx.Dr6=0;
                    if(bot && kind==0 && (source==2 || source==3))
                    {
                        selected=true;
                        if(SuspendThread(game.hThread)==DWORD(-1)) { std::printf("ERROR Suspend %lu\n",GetLastError()); break; }
                        held=true; ctx.Dr0=0; ctx.Dr7=0;
                        std::printf("SELECTED_REAL_BOT_CALL main_thread_held_for_install=1\n");
                        Entry(game.hProcess,base+CHECKED_RVA,"wrapper_at_call");
                    }
                    if(!SetThreadContext(game.hThread,&ctx)) break;
                }
                else continuation=DBG_EXCEPTION_NOT_HANDLED;
            }
            else if(code!=EXCEPTION_BREAKPOINT && code!=0x40010006 && code!=0x4001000a)
            {
                std::printf("EXCEPTION code=%08lX firstChance=%lu\n",code,ev.u.Exception.dwFirstChance);
                continuation=DBG_EXCEPTION_NOT_HANDLED;
            }
        }
        else if(ev.dwDebugEventCode==OUTPUT_DEBUG_STRING_EVENT)
        {
            const auto& ds=ev.u.DebugString;
            SIZE_T size=SIZE_T(ds.nDebugStringLength)*(ds.fUnicode?2:1),got=0;
            std::vector<char> data(size+2,0);
            if(ReadProcessMemory(game.hProcess,ds.lpDebugStringData,data.data(),size,&got))
            {
                std::string text;
                if(ds.fUnicode)
                {
                    int n=WideCharToMultiByte(CP_UTF8,0,reinterpret_cast<const wchar_t*>(data.data()),int(got/2),nullptr,0,nullptr,nullptr);
                    text.resize(n);
                    WideCharToMultiByte(CP_UTF8,0,reinterpret_cast<const wchar_t*>(data.data()),int(got/2),text.data(),n,nullptr,nullptr);
                }
                else text.assign(data.data(),got);
                std::printf("DEBUG ms=%llu %s",GetTickCount64()-start,text.c_str());
                if(!selected) {
                    CONTEXT verify{}; verify.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                    if(GetThreadContext(game.hThread,&verify)) std::printf("HW_LATER dr0=%llX dr7=%llX\n",(unsigned long long)verify.Dr0,(unsigned long long)verify.Dr7);
                }
                if(text.find("[HK-01] Hook installed.")!=std::string::npos)
                {
                    installed=Entry(game.hProcess,base+SPAWN_RVA,"spawn_hook_installed");
                    Entry(game.hProcess,base+CHECKED_RVA,"wrapper_after_install");
                    if(installed && held)
                    {
                        const DWORD count=ResumeThread(game.hThread);
                        std::printf("RESUME_REAL_BOT_CALL previous_suspend_count=%lu\n",count);
                        if(count!=DWORD(-1)) held=false;
                    }
                }
                if(text.find("HK01_B")!=std::string::npos)
                {
                    bDetected=true;
                    Entry(game.hProcess,base+SPAWN_RVA,"spawn_at_B_detection");
                }
            }
        }
        else if(ev.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)
        {
            finished=true; gameExit=ev.u.ExitProcess.dwExitCode;
        }
        ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,continuation);
        if(finished) break;
        if(selected && !injector.hProcess)
        {
            const std::wstring command=L"\""+cwd+L"\\injector.exe\" "+std::to_wstring(game.dwProcessId)+L" \""+cwd+L"\\attack.dll\" -";
            const bool ok=Launch(command,cwd,0,injector);
            std::printf("INJECTION_LAUNCHED success=%d target_pid=%lu\n",ok,game.dwProcessId);
            if(!ok) break;
        }
        std::fflush(stdout);
    }
    if(!finished)
    {
        if(held) ResumeThread(game.hThread);
        TerminateProcess(game.hProcess,77);
        const auto stop=GetTickCount64();
        while(GetTickCount64()-stop<3000)
        {
            DEBUG_EVENT ev{};
            if(!WaitForDebugEvent(&ev,100)) continue;
            const bool exit=ev.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT;
            if(exit) gameExit=ev.u.ExitProcess.dwExitCode;
            ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,DBG_CONTINUE);
            if(exit) break;
        }
        std::printf("CLEANUP external_termination=1 exit=%lu\n",gameExit);
    }
    if(injector.hProcess)
    {
        if(WaitForSingleObject(injector.hProcess,2000)!=WAIT_OBJECT_0) TerminateProcess(injector.hProcess,77);
        DWORD code=STILL_ACTIVE;
        if(GetExitCodeProcess(injector.hProcess,&code)) std::printf("INJECTOR_EXIT=%lu\n",code);
        CloseHandle(injector.hThread); CloseHandle(injector.hProcess);
    }
    std::printf("FINAL_B_RESULT bot_checked_call=%d actual_E9=%d B_debug=%d game_exit=%lu self_exit=%d\n",selected,installed,bDetected,gameExit,finished);
    CloseHandle(game.hThread); CloseHandle(game.hProcess);
    return selected && installed && bDetected && finished && gameExit==2 ? 0 : 2;
}
