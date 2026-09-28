/*
** Winamp for Linux: minimal stand-in for Src/Wasabi/bfc/dispatch.h.
** Same calling convention (message id + array of pointers to arguments),
** written with variadic templates. Enough for the tagz interfaces.
*/
#pragma once
#include <bfc/platform/types.h>

class Dispatchable
{
public:
	virtual int _dispatch(int msg, void *retval, void **params = 0, int nparam = 0) = 0;

protected:
	int _voidcall(int msg) { return _dispatch(msg, 0); }

	template <class... P>
	int _voidcall(int msg, P... p)
	{
		void *params[] = { (void *)&p... };
		return _dispatch(msg, 0, params, (int)sizeof...(P));
	}

	template <class R>
	R _call(int msg, R defval)
	{
		R retval;
		if (_dispatch(msg, &retval)) return retval;
		return defval;
	}

	template <class R, class... P>
	R _call(int msg, R defval, P... p)
	{
		void *params[] = { (void *)&p... };
		R retval;
		if (_dispatch(msg, &retval, params, (int)sizeof...(P))) return retval;
		return defval;
	}
};

#define DISPATCH_CODES enum
#define RECVS_DISPATCH virtual int _dispatch(int msg, void *retval, void **params = 0, int nparam = 0)
#define START_DISPATCH \
	int CBCLASS::_dispatch(int msg, void *retval, void **params, int nparam) { \
		(void)nparam; switch (msg) {
#define END_DISPATCH \
		default: return 0; \
		} \
		return 1; \
	}
