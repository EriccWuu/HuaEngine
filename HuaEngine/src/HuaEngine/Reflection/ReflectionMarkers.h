#pragma once

#if defined(HE_META_SCANNING) && defined(__clang__)
#define sattr(...) clang::annotate("hua.sattr:" #__VA_ARGS__)
#else
#define sattr(...)
#endif
