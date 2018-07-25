
bool FUNCNAME(CScript const& scriptSig)
{
    CC *cond;
    if (!(cond = GetCryptoCondition(scriptSig)))
        return false;
    // Recurse the CC tree to find asset condition
    auto findEval = [] (CC *cond, struct CCVisitor _) {
    bool r = cc_typeId(cond) == CC_Eval && cond->codeLength == 1 && cond->code[0] == EVALCODE;
    // false for a match, true for continue
    return r ? 0 : 1;
    };
    CCVisitor visitor = {findEval, (uint8_t*)"", 0, NULL};
    bool out =! cc_visit(cond, visitor);
    cc_free(cond);
    return out;
}
