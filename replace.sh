sed -i '' "s/ZCash/DeepWebCash/g" `grep ZCash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i '' "s/ZCASH/DWCASH/g" `grep ZCASH -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i '' "s/z\.cash/dw\.cash/g" `grep z\.cash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i '' "s/zcash/dwcash/g" `grep zcash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i '' "s/Zcash/DeepWebCash/g" `grep Zcash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i '' "s/github.com\/dwcash\/dwcash/github.com\/deepwebcash\/deepwebcash/g" `grep github\.com\/dwcash\/dwcash -rl --exclude-dir=.git --exclude=replace.sh .`