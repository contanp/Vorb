//
// Script.h
// Vorb Engine
//
// Created by Cristian Zaloj on 22 Feb 2015
// Copyright 2014 Regrowth Studios
// All Rights Reserved
//

/*! \file Script.h
 * @brief Operations that can be performed on scripting objects.
 */

#pragma once

#ifndef Vorb_Script_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_Script_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "ScriptValueSenders.h"
#include "Environment.h"

namespace vorb {
    namespace script {
        class Environment;
        class Function;

        namespace impl {
            void pushToTop(EnvironmentHandle h, const Function& f);
            void call(EnvironmentHandle h, size_t n, size_t r);
            void popStack(EnvironmentHandle h);
            void* popUpvalueObject(EnvironmentHandle h);

            template<typename Arg>
            void pushArgs(EnvironmentHandle h, Arg a) {
                ScriptValueSender<Arg>::push(h, a);
            }
            template<typename Arg, typename... Args>
            void pushArgs(EnvironmentHandle h, Arg a, Args... other) {
                ScriptValueSender<Arg>::push(h, a);
                pushArgs<Args...>(h, other...);
            }

            template<typename Arg>
            Arg popValue(EnvironmentHandle h) {
                Arg a = ScriptValueSender<Arg>::pop(h);
                popStack(h);
                return a;
            }

            template<typename F, typename... Args>
            void cCall(EnvironmentHandle h, F func) {
                func(popValue<Args>(h)...);
            }
            template<typename... Args>
            void fCall(EnvironmentHandle h, void* del) {
                typedef RDelegate<void, Args...>* FunctionPtr;
                FunctionPtr f = (FunctionPtr)del;
                f->invoke(popValue<Args>(h)...);
            }
            template<typename Ret, typename... Args>
            void fRCall(EnvironmentHandle h, void* del) {
                typedef RDelegate<Ret, Args...>* FunctionPtr;
                FunctionPtr f = (FunctionPtr)del;
                Ret retValue = f->invoke(popValue<Args>(h)...);
                ScriptValueSender<Ret>::push(h, retValue);
            }

            template<typename F, F f, typename... Args>
            int luaCall(lua_State* s) {
                cCall<F, Args...>(s, f);
                return 0;
            }
            template<typename... Args>
            int luaDCall(lua_State* s) {
                void* del = popUpvalueObject(s);
                fCall<Args...>(s, del);
                return 0;
            }
            template<typename Ret, typename... Args>
            int luaDRCall(lua_State* s) {
                void* del = popUpvalueObject(s);
                fRCall<Ret, Args...>(s, del);
                return 1;
            }
        }

        template<typename... Args>
        bool call(Environment& env, const nString& f, Args... args) {
            Function& scr = env[f];
            EnvironmentHandle hnd = env.getHandle();

            impl::pushToTop(hnd, scr);
            impl::pushArgs<Args...>(hnd, args...);
            impl::call(hnd, sizeof...(Args), 0);
            return true;
        }
        template<typename Ret, typename... Args>
        bool callReturn(Environment& env, const nString& f, OUT Ret* retValue, Args... args) {
            Function& scr = env[f];
            EnvironmentHandle hnd = env.getHandle();

            impl::pushToTop(hnd, scr);
            impl::pushArgs<Args...>(hnd, args...);
            impl::call(hnd, sizeof...(Args), 1);
            *retValue = impl::popValue<Ret>(hnd);
            return true;
        }

        typedef int(*ScriptFunc)(EnvironmentHandle s);
        template<typename F, F f, typename... Args>
        ScriptFunc fromFunction() {
            return impl::luaCall<F, f, Args...>;
        }
        typedef int(*ScriptFunc)(EnvironmentHandle s);
        template<typename... Args>
        ScriptFunc fromDelegate() {
            return impl::luaDCall<Args...>;
        }
        template<typename Ret, typename... Args>
        ScriptFunc fromRDelegate() {
            return impl::luaDRCall<Ret, Args...>;
        }
    }
}
namespace vscript = vorb::script;

#endif // !Vorb_Script_h__
