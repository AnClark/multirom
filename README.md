# AnClark's MultiROM Distribution
This is one of the distributions of MultiROM. Compared with Tassadar's original version, I am trying to add some essential features to make it support more devices besides Nexus series.

## What is MultiROM?
MultiROM is a one-of-a-kind multi-boot solution **written by Tassadar**. It can boot android ROM while
keeping the one in internal memory intact or boot Ubuntu without formating
the whole device. MultiROM can boot either from internal memory of the device
or from USB flash drive.

## What am I taking efforts with?
### ENCRYPTION
Due to Google's policy, nowadays encryption is already a MUST for phones using Android 6.0 or newer, and the most popular solution is Qualcomm's QSEECOM.

From code, we can know that Tassadar has already written an interface for encryption, but it cannot support encryption well. When testing MultiROM with those devices using QSEECOM, MultiROM dies, as currently MultiROM **DOESN'T SUPPORT QSEECOM AT ALL**. Developer @zhuowei provided his solution a year ago, but Tassadar didn't apply.

So here's my work: let MultiROM support QSEECOM. My device is Xiaomi Max with Qualcomm Snapdragon 652, and I think if I were succeed, my efforts would also fit other devices using Qualcomm's solution.

## Notice
Don't forget that MultiROM is open-source, so I sincerely expect that all developers love MultiROM can join, and make it much better!


## Official build guides
### Sources
MultiROM uses git submodules, so you need to clone them as well:

    git clone https://github.com/Tasssadar/multirom.git system/extras/multirom
    cd system/extras/multirom
    git submodule update --init

It also needs libbootimg:

    git clone https://github.com/Tasssadar/libbootimg.git system/extras/libbootimg

###Build
Clone repo to folder `system/extras/multirom` inside Android 4.x source tree.
You can find device folders on my github, I currently use OmniROM tree for
building (means branch android-4.4-mrom in device repos).
MultiROM also needs libbootimg (https://github.com/Tasssadar/libbootimg)
in folder `system/extras/libbootimg`. Use something like this to build:

    . build/envsetup.sh
    lunch full_grouper-userdebug
    make -j4 multirom trampoline

To build installation ZIP file, use `multirom_zip` target:

    make -j4 multirom_zip
