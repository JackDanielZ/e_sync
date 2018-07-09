#Edje
/opt/e/bin/edje_cc -v -id ./images e-module.edc e-module.edj
[ $? -eq 0 ] || exit 1
/opt/e/bin/edje_cc -v -id ./images icon.edc icon.edj
[ $? -eq 0 ] || exit 1

# Test app
gcc -g src/e_mod_main.c $CFLAGS `pkg-config --cflags --libs elementary` -o src/e_sync
[ $? -eq 0 ] || exit 1
