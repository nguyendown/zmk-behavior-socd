# SOCD Behavior

SOCD cleaner for ZMK.

## Build

See [Building With Modules](https://zmk.dev/docs/features/modules#building-with-modules).

Edit `zmk-config/config/west.yml`.

```diff
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
+    - name: nguyendown
+      url-base: https://github.com/nguyendown
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
+    - name: zmk-behavior-socd
+      remote: nguyendown
+      revision: main
  self:
    path: config
```

## Keymap

`&socd` is the default SOCD instance.
It is Last Input Priority (LIP) and uses key press binding `&kp`.

Add this to `.keymap` to use `&socd`.

```diff
#include <behaviors.dtsi>
+#include <behaviors/socd.dtsi>
#include <dt-bindings/zmk/keys.h>
```

The following keymap prevents A and D from overlapping each other.

```dts
/ {
    keymap {
        default_layer {
            bindings = <
                &kp   Q &kp W &kp   E
                &socd A &kp S &socd D>;
        };
    };
};
```

For W and S, create a new SOCD instance.
See [Multiple instances](#multiple-instances)

## Config

### `first-input-priority`

When `first-input-priority` is set,
SOCD behavior will switch to First Input Priority (FIP).

```dts
&socd {
    first-input-priority;
};

/ {
    keymap {
        ...
    };
};
```


### `absolute-priority`

The following example gives `&socd A` absolute priority.

```dts
&socd {
    absolute-priority = <A>;
};
```

### `no-resume`

When `no-resume` is set, SOCD behavior will not resume the remaining pressed key.

### `neutral`

When `neutral` is set, pressing two keys at the same time
results in no input being registered.

This also overrides all the properties mentioned above.
The behavior does not prioritize any input,
whether it is FIP, LIP or absolute priority.
The remaining pressed key always resumes even if `no-resume` is set.

## Multiple instances

The following example creates a new SOCD instance that works separately from the default `&socd`.

```dts
/ {
    behaviors {
        socd2: socd2 {
            compatible = "zmk,behavior-socd";
            #binding-cells = <1>;
            bindings = <&kp>;
        };
    };

    keymap {
        default_layer {
            bindings = <
                &kp   Q &socd2 W &kp   E
                &socd A &socd2 S &socd D>;
        };
    };
};
```

## Test

```shell
cd path/to/zmk-behavior-socd
scripts/run-test.sh
```

For a specific test case.

```shell
scripts/run-test.sh tests/wasd
```
