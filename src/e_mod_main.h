#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#ifdef EAPI
#undef EAPI
#endif
#define EAPI __attribute__ ((visibility("default")))

#endif
