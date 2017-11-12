#ifndef SIGNALTK_H_
#define SIGNALTK_H_

#include <common/Common.h>

#include <linux/signal.h>


// Note: SIGKILL will always be allowed by SignalTk_blockSignals()
#define SIGNALTK_ALLOWED_SIGNALS \
   (sigmask(SIGTERM) | sigmask(SIGQUIT) | sigmask(SIGHUP) )


static inline void SignalTk_blockSignals(bool allowSigInt, sigset_t* outOldSignalSet);
static inline void SignalTk_restoreSignals(sigset_t* oldSignalSet);

static inline bool SignalTk_containsSignal(sigset_t* signalSet, int signum);
static inline void SignalTk_enableSigInt(void);


/**
 * Block certain signals of current process. Intended to temporarily avoid signals that would have a
 * bad effect e.g. during remoting.
 *
 * Note: Do not forget to call __SignalTk_restoreSignals() later.
 *
 * @param allowSigInt usually true (false probably only useful for logging).
 * @param outOldSignalSet the original set of blocked signals before we modified it.
 */
void SignalTk_blockSignals(bool allowSigInt, sigset_t* outOldSignalSet)
{
   unsigned long allowedSigs = sigmask(SIGKILL) | SIGNALTK_ALLOWED_SIGNALS;
   unsigned long allowedSigsPlusInt = allowedSigs | sigmask(SIGINT);

   unsigned long newAllowedSigs =
      allowSigInt ? allowedSigsPlusInt : allowedSigs;

   sigset_t newSigmask;


   // set inverted mask (we have allowed sigs, but need the mask of not allowed signals)
   siginitsetinv(&newSigmask, newAllowedSigs);

   // block signals
   sigprocmask(SIG_BLOCK, &newSigmask, outOldSignalSet);
}

void SignalTk_restoreSignals(sigset_t* oldSignalSet)
{
   sigprocmask(SIG_SETMASK, oldSignalSet, NULL);
}

/**
 * Find out whether given set contains a certain signal.
 *
 * note: the signal mask of a process is usually the set of blocked signals, so if you pass a
 * process signal mask and this returns true, it means the signal was previously blocked.
 *
 * @param signum signal number, e.g. SIGINT.
 * @return true if set contains given signal.
 */
bool SignalTk_containsSignal(sigset_t* signalSet, int signum)
{
   return (sigismember(signalSet, signum) == 1);
}

/**
 * Enable interrupt signal for current process.
 */
void SignalTk_enableSigInt(void)
{
   sigset_t newSigmask;

   // invert signal mask (we actually need the blocked signals)
   siginitset(&newSigmask, sigmask(SIGINT) );

   // unblock signals
   sigprocmask(SIG_UNBLOCK, &newSigmask, NULL);
}

#endif /*SIGNALTK_H_*/
