# Datasheets

Direct manufacturer links (as of 2026-07). For dead links: search part number +
"datasheet"; distributor mirrors (DigiKey/Mouser) are usually the most stable.

## Microcontrollers

| Chip | Boards | Datasheet | Reference Manual |
| --- | --- | --- | --- |
| STM32L476RG | BTC, EXP1–3 | [DS10198](https://www.st.com/resource/en/datasheet/stm32l476rg.pdf) | [RM0351](https://www.st.com/resource/en/reference_manual/rm0351-stm32l47xxx-stm32l48xxx-stm32l49xxx-and-stm32l4axxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) |
| STM32H753ZI | Test Nucleo | [DS12110](https://www.st.com/resource/en/datasheet/stm32h753zi.pdf) | [RM0433](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) |
| STM32U031F8 (EXP3 wired stack) | EXP3 | [DS14560](https://www.st.com/resource/en/datasheet/stm32u031f8.pdf) | [RM0503](https://www.st.com/resource/en/reference_manual/rm0503-stm32u0-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) |

## Sensors

| Chip | Driver | Boards | Datasheet |
| --- | --- | --- | --- |
| MS5611-01BA03 (Baro) | `shared/sensors/ms5611.cpp` | all | [TE MS5611-01BA03](https://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocNm=MS5611-01BA03&DocType=Data+Sheet&DocLang=English) |
| TMP117 (Temperature) | `shared/sensors/tmp117.cpp` | all | [TI TMP117](https://www.ti.com/lit/ds/symlink/tmp117.pdf) |
| ICM-42686-P (IMU ±32g) | `shared/sensors/icm42686.cpp` | BTC, EXP3 | [TDK DS-000639](https://invensense.tdk.com/download-pdf/icm-42686-p-datasheet/) |
| ICM-42688-P (IMU ±16g) | `shared/sensors/icm42688.cpp` | — (driver available) | [TDK DS-000347](https://invensense.tdk.com/download-pdf/icm-42688-p-datasheet/) |
| AS7265x Spectral Triad | `shared/sensors/as7265x.cpp` | EXP1 (SparkFun SEN-15050) | [ams AS7265x (SparkFun mirror)](https://cdn.sparkfun.com/assets/c/2/9/0/a/AS7265x_Datasheet.pdf), [Hookup Guide](https://learn.sparkfun.com/tutorials/spectral-triad-as7265x-hookup-guide) |
| MMC5983MA (Magnetometer) | planned (EXP3 stacks) | EXP3 | [Memsic Rev A](https://www.memsic.com/Public/Uploads/uploadfile/files/20220119/MMC5983MADatasheetRevA.pdf), [DigiKey mirror](https://media.digikey.com/pdf/Data%20Sheets/MEMSIC%20PDFs/MMC5983MA_RevA_4-3-19.pdf) |

## Actuators / Misc

| Chip | Driver | Boards | Datasheet |
| --- | --- | --- | --- |
| LP5810 (LED driver) | `shared/led/lp5810.cpp` | EXP1 (RGB 0x58, UV/IR 0x5C) | [TI LP5810](https://www.ti.com/lit/ds/symlink/lp5810.pdf) |
| Cree CLQ6A-FKW (RGB LED, PLCC8) | — | EXP1 | [Cree HB-CLQ6A-FKW](https://downloads.cree-led.com/files/ds/h/HB-CLQ6A-FKW.pdf) |
