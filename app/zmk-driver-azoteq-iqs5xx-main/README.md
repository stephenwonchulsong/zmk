# ZMK driver for Azoteq IQS5XX trackpads

## Compatibility

This driver should work with any IQS5XX based trackpad. However, it has only been tested and known to work with the following models:

- TPS43 (Reached EOL)

Feel free to send a pull request if you test with any of the following models:

- TPS65 (Reached EOL)
- TPR40
- TPR48
- TPR54
- TPE60
- TPE48
- TPS48

## Supported features

- Trackpad movement.
- Single finger tap: Reported as a left click.
- Two finger tap: Reported as a right click.
- Press and hold: Reported as a continuos left click (allows click and drag).
- Vertical scroll.
- Horizontal scroll.

## Usage

- Specify a node with the "azoteq,iqs5xx" compatible inside an i2c node in your keyboard overlay.
- Reference it from an input listener:

```
/ {
    tps43_input: tps43_input {
        compatible = "zmk,input-listener";
        device = <&tps43>;
    };
};

&arduino_i2c {
    status = "okay";
    tps43: iqs5xx@74 {
        status = "okay";
        compatible = "azoteq,iqs5xx";
        reg = <0x74>;

        reset-gpios = <&arduino_header 14 GPIO_ACTIVE_LOW>;
        rdy-gpios = <&arduino_header 15 GPIO_ACTIVE_HIGH>;

        /*
         * Potentially non-exhaustive list of configuration options.
         * See: dts/bindings/input/azoteq,iqs5xx-common.yaml for a full list.
         */
        one-finger-tap;
        press-and-hold;
        press-and-hold-time = <250>;
        two-finger-tap;

        scroll;
        natural-scroll-y;
        natural-scroll-x;

        bottom-beta = <5>;
        stationary-threshold = <5>;

        switch-xy;
    };
};
```

