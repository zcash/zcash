/******************************************************************************
* Copyright © 2014-2019 The SuperNET Developers.                             *
*                                                                            *
* See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
* the top-level directory of this distribution for the individual copyright  *
* holder information and the developer policies on copyright and licensing.  *
*                                                                            *
* Unless otherwise agreed in a custom licensing agreement, no part of the    *
* SuperNET software, including this file may be copied, modified, propagated *
* or distributed except according to the terms contained in the LICENSE file *
*                                                                            *
* Removal or modification of this copyright notice is prohibited.            *
*                                                                            *
******************************************************************************/

// cjsonpointer.h
// C-language RFC 6901 JSON pointer implementation for cJSON parser
#ifndef __CJSONPOINTER_H__
#define __CJSONPOINTER_H__

#include <string>
#include "cJSON.h"
const cJSON *SimpleJsonPointer(const cJSON *json, const char *pointer, std::string &serror);

#endif // #ifndef __CJSONPOINTER_H__