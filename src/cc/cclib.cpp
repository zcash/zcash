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

#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "cc/CCinclude.h"
#include "main.h"
#include "chain.h"
#include "core_io.h"
#include "crosschain.h"

struct CClib_rpcinfo
{
    char *method,*help;
    int32_t numrequiredargs,maxargs; // frontloaded with required
    uint8_t funcid;
}
CClib_methods[] =
{
    { "faucet2_get", "<no args>", 0, 0, 'G' },
};

std::string MYCCLIBNAME = (char *)"stub";

char *CClib_name() { return(MYCCLIBNAME); }

std::string CClib_rawtxgen(struct CC_info *cp,uint8_t funcid,cJSON *params);

UniValue CClib_info(struct CC_info *cp)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i; char str[2];
    result.push_back(Pair("result","success"));
    result.push_back(Pair("CClib",CClib_name()));
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        UniValue obj(UniValue::VOBJ);
        if ( CClib_methods[i].funcid < ' ' || CClib_methods[i].funcid >= 128 )
            obj.push_back(Pair("funcid",CClib_methods[i].funcid));
        else
        {
            str[0] = CClib_methods[i].funcid;
            str[1] = 0;
            obj.push_back(Pair("funcid",str));
        }
        obj.push_back(Pair("name",CClib_methods[i].method));
        obj.push_back(Pair("help",CClib_methods[i].help));
        obj.push_back(Pair("params_required",CClib_methods[i].numrequiredargs));
        obj.push_back(Pair("params_max",CClib_methods[i].maxargs));
        a.push_back(obj));
    }
    result.push_back(Pair("methods",a));
    return(result);
}

UniValue CClib(struct CC_info *cp,char *method,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t i; std::string rawtx;
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        if ( strcmp(method,CClib_methods[i].method) == 0 )
        {
            result.push_back(Pair("result","success"));
            result.push_back(Pair("method",CClib_methods[i].method));
            rawtx = CClib_rawtxgen(cp,CClib_methods[i].funcid,params);
            result.push_back(Pair("rawtx",rawtx));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("method",CClib_methods[i].method));
    result.push_back(Pair("error","method not found"));
    return(result);
}

bool CClib_validate(struct CC_info *cp,Eval *eval,const CTransaction &txTo,unsigned int nIn)
{
    return(true); // for now
}

std::string CClib_rawtxgen(struct CC_info *cp,uint8_t funcid,cJSON *params)
{
    return((char *)"deadbeef");
}
