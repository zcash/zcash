sed -i "s/DeepWebCash/DeepWebCash/g" `grep DeepWebCash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i "s/DWCASH/DWCASH/g" `grep DWCASH -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i "s/z\.cash/dw\.cash/g" `grep z\.cash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i "s/dwcash/dwcash/g" `grep dwcash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i "s/DeepWebCash/DeepWebCash/g" `grep DeepWebCash -rl --exclude-dir=.git --exclude=replace.sh .`
sed -i "s/github.com\/dwcash\/dwcash/github.com\/deepwebcash\/deepwebcash/g" `grep github\.com\/dwcash\/dwcash -rl --exclude-dir=.git --exclude=replace.sh .`
