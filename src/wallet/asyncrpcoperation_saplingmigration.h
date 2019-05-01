#include "amount.h"
#include "asyncrpcoperation.h"
#include "univalue.h"
#include "zcash/Address.hpp"
#include "zcash/zip32.h"

class AsyncRPCOperation_saplingmigration : public AsyncRPCOperation
{
public:
    AsyncRPCOperation_saplingmigration(int targetHeight);
    virtual ~AsyncRPCOperation_saplingmigration();

    // We don't want to be copied or moved around
    AsyncRPCOperation_saplingmigration(AsyncRPCOperation_saplingmigration const&) = delete;            // Copy construct
    AsyncRPCOperation_saplingmigration(AsyncRPCOperation_saplingmigration&&) = delete;                 // Move construct
    AsyncRPCOperation_saplingmigration& operator=(AsyncRPCOperation_saplingmigration const&) = delete; // Copy assign
    AsyncRPCOperation_saplingmigration& operator=(AsyncRPCOperation_saplingmigration&&) = delete;      // Move assign

    static libzcash::SaplingPaymentAddress getMigrationDestAddress(const HDSeed& seed);

    virtual void main();

    virtual UniValue getStatus() const;

private:
    int targetHeight_;

    bool main_impl();

    void setMigrationResult(int numTxCreated);

    CAmount chooseAmount(const CAmount& availableFunds);
};
