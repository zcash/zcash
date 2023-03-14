def exact_div(x, y):
    assert x % y == 0
    return x // y

# floor(u/x + v/y)
def div2(u, x, v, y):
    return (u*y + v*x) // (x*y)

TESTNET = 0
MAINNET = 1

class Network:
    # <https://zips.z.cash/protocol/protocol.pdf#constants>
    def __init__(self, network):
        self.BlossomActivationHeight = 653600 if network == MAINNET else 584000

    SlowStartInterval = 20000
    MaxBlockSubsidy = 1250000000 # 12.5 ZEC
    PreBlossomHalvingInterval = 840000
    PreBlossomPoWTargetSpacing = 150
    PostBlossomPoWTargetSpacing = 75

    # <https://zips.z.cash/protocol/protocol.pdf#diffadjustment>
    def IsBlossomActivated(self, height):
        return height >= self.BlossomActivationHeight

    BlossomPoWTargetSpacingRatio = exact_div(PreBlossomPoWTargetSpacing, PostBlossomPoWTargetSpacing)

    # no need for floor since this is necessarily an integer
    PostBlossomHalvingInterval = PreBlossomHalvingInterval * BlossomPoWTargetSpacingRatio

    # <https://zips.z.cash/protocol/protocol.pdf#subsidies>
    SlowStartShift = exact_div(SlowStartInterval, 2)

    SlowStartRate = exact_div(MaxBlockSubsidy, SlowStartInterval)

    SupplyCache = []

    def Halving(self, height):
        if height < self.SlowStartShift:
            return 0
        elif not self.IsBlossomActivated(height):
            return (height - self.SlowStartShift) // self.PreBlossomHalvingInterval
        else:
            return div2(self.BlossomActivationHeight - self.SlowStartShift, self.PreBlossomHalvingInterval,
                        height - self.BlossomActivationHeight, self.PostBlossomHalvingInterval)

    def BlockSubsidy(self, height):
        if height < self.SlowStartShift:
            return self.SlowStartRate * height
        elif self.SlowStartShift <= height and height < self.SlowStartInterval:
            return self.SlowStartRate * (height + 1)
        if self.SlowStartInterval <= height and not self.IsBlossomActivated(height):
            return self.MaxBlockSubsidy // (1 << self.Halving(height))
        else:
            return self.MaxBlockSubsidy // (self.BlossomPoWTargetSpacingRatio << self.Halving(height))

    def SupplyAfterHeight(self, height):
        cacheLen = len(self.SupplyCache)
        if cacheLen > height:
            return self.SupplyCache[height]
        else:
            cur = 0 if cacheLen == 0 else self.SupplyCache[-1]
            for h in range(cacheLen, height + 1):
                cur += self.BlockSubsidy(h)
                self.SupplyCache.append(cur)
            return self.SupplyCache[-1]
