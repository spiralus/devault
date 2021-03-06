// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "chainparams.h"
#include "clientversion.h"
#include "compat.h"
#include "config.h"
#include "httprpc.h"
#include "httpserver.h"
#include "init.h"
#include "noui.h"
#include "rpc/server.h"
#include "fs_util.h"
#include "utilstrencodings.h"
#include "walletinitinterface.h"
#include "support/allocators/secure.h"

#include <cstdint>
#include <thread>
#include <cstdio>

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of DeVault
 * (https://www.devault.cc/). DeVault Core is a client for the digital
 * currency called Devault, which enables, instant payments to anyone, anywhere in the world.
 * DeVault uses peer-to-peer technology to operate with no central authority: managing
 * transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the
 * MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or
 * <code>Files</code> at the top of the page to start navigating the code.
 */

void WaitForShutdown() {
    while (!ShutdownRequested()) {
        MilliSleep(200);
    }
    Interrupt();
}

void getPassphrase(SecureString& walletPassphrase) {
  std::cout << "Enter a Wallet Encryption password (at least 4 characters and upto 40 characters long)\n";
  SecureString pass1;
  SecureString pass2;
  int char_count=0; // This is used to handle Ctrl-C which would create
  // an infinite loop and crash below otherwise
  char c='0';
  const int min_char_count = 2*4 + 1;
  do {
    do {
      std::cin.get(c);
      char_count++;
      if (char_count++ > 81) {
        // Don't print message, just exit because it can be due to Ctrl-C
        exit(0);
      }
      if (c != '\n') pass1.push_back(c);
    } while (c != '\n');
    if (char_count < min_char_count) {
      std::cout << "Password must be at least 4 characters long, please restart and retry\n";
      exit(0);
    }
  } while (char_count < min_char_count);
  c = '0';
  char_count = 0;
  std::cout << "Confirm password\n";
  while (c != '\n') {
    std::cin.get(c);
    if (char_count++ > 81) exit(0);
    if (c != '\n') pass2.push_back(c);
  }
  if (pass1 != pass2) {
    std::cout << "Passwords don't match, please restart and retry\n";
    exit(0);
  }
  walletPassphrase   = pass1;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char *argv[]) {
    // FIXME: Ideally, we'd like to build the config here, but that's currently
    // not possible as the whole application has too many global state. However,
    // this is a first step.
    auto &config = const_cast<Config &>(GetConfig());
    RPCServer rpcServer;
    HTTPRPCRequestProcessor httpRPCRequestProcessor(config, rpcServer);

    bool fRet = false;
    SecureString walletPassphrase;
    //
    // Parameters
    //
    // If Qt is used, parameters/devault.conf are parsed in qt/bitcoin.cpp's
    // main()
    gArgs.ParseParameters(argc, argv);

    // Process help and version before taking care about datadir
    if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        std::string strUsage = strprintf(_("%s Daemon"), _(PACKAGE_NAME)) +
                               " " + _("version") + " " + FormatFullVersion() +
                               "\n";

        if (gArgs.IsArgSet("-version")) {
            strUsage += FormatParagraph(LicenseInfo());
        } else {
            strUsage += "\n" + _("Usage:") + "\n" +
                        "  devaultd [options]                     " +
                        strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";

            strUsage += "\n" + HelpMessage(HelpMessageMode::BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try {
      
        if (!fs::is_directory(GetDataDir(false))) {
            fprintf(stderr,
                    "Error: Specified data directory \"%s\" does not exist.\n",
                    gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        try {
            gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));
        } catch (const std::exception &e) {
            fprintf(stderr, "Error reading configuration file: %s\n", e.what());
            return false;
        }
        // Check for -testnet or -regtest parameter (Params() calls are only
        // valid after this clause)
        try {
            SelectParams(gArgs.GetChainName());
        } catch (const std::exception &e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }
      
        // Error out when loose non-argument tokens are encountered on command
        // line
        for (int i = 1; i < argc; i++) {
            if (!IsSwitchChar(argv[i][0])) {
                fprintf(stderr,
                        "Error: Command line contains unexpected token '%s', "
                        "see devaultd -h for a list of options.\n",
                        argv[i]);
                exit(EXIT_FAILURE);
            }
        }

        // -server defaults to true for bitcoind but not for the GUI so do this
        // here
        gArgs.SoftSetBoolArg("-server", true);
        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();
        if (!AppInitBasicSetup()) {
            // InitError will have been called with detailed error, which ends
            // up on console
            exit(1);
        }
        if (!AppInitParameterInteraction(config, rpcServer)) {
            // InitError will have been called with detailed error, which ends
            // up on console
            exit(1);
        }

#ifdef ENABLE_WALLET        
      if (!g_wallet_init_interface->CheckIfWalletExists(config.GetChainParams())) {
        getPassphrase(walletPassphrase);
      }
#endif

        if (!AppInitSanityChecks()) {
            // InitError will have been called with detailed error, which ends
            // up on console
            exit(1);
        }
        if (gArgs.GetBoolArg("-daemon", false)) {
#if HAVE_DECL_DAEMON
            fprintf(stdout, "DeVault server starting\n");

            // Daemonize
            if (daemon(1, 0)) {
                // don't chdir (1), do close FDs (0)
                fprintf(stderr, "Error: daemon() failed: %s\n",
                        strerror(errno));
                return false;
            }
#else
            fprintf(
                stderr,
                "Error: -daemon is not supported on this operating system\n");
            return false;
#endif // HAVE_DECL_DAEMON
        }

        // Lock data directory after daemonization
        if (!AppInitLockDataDirectory()) {
            // If locking the data directory failed, exit immediately
            exit(EXIT_FAILURE);
        }
        fRet = AppInitMain(config, httpRPCRequestProcessor, walletPassphrase);
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    if (!fRet) {
        Interrupt();
    } else {
        WaitForShutdown();
    }
    Shutdown();

    return fRet;
}

int main(int argc, char *argv[]) {
    SetupEnvironment();

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
