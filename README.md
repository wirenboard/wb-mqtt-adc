wb-mqtt-adc
==========
Драйвер ADC запускается как сервис через system.d.
При запуске драйвер считывает конфигурацию для каналов с файла `/etc/wb-mqtt-adc.conf` и из `.conf` файлов в каталоге `/etc/wb-mqtt-adc.conf.d`. Приоритетными считаются настройки в `/etc/wb-mqtt-adc.conf`.
Ниже приведен пример конфигурационного файла с объяснением параметров.
```

{
    // Под каким именем будет опубликовано устройство в MQTT
    // то есть ../meta/name ADCs
    "device_name" : "ADCs",
    "debug" : false,
    "iio_channels" : [
         {
                // под каким id будет публиковаться данный канал в MQTT
                // то есть /devices/wb-adc/controls/ID
                "id" : "A1",

                // указывает по скольки выборкам происходит усреднение значения
                "averaging_window": 1,

                // номер физического канала, указывает с какого файла будет читаться значение :
                // /bus/iio/devices/iio:device0/in_voltage4_raw , т.е. in_voltageНОМЕРКАНАЛА_raw
                "channel_number" : 4,

                // коэффициент перевода из условных величин, измеряемых АЦП в реальное напряжение
                "voltage_multiplier" : 8.3917,
    
                // указывает сколько значащих цифр после запятой выводить у данного канала
                // по умолчанию 3
                "decimal_places" : 2,

                // режим работы драйвера однопоточный, данное значение указывает
                // сколько раз считать значение в данном канале за один проход,
                // по умолчанию 10
                "readings_number" : 10,

                // указывает максимальное значение напряжение, которое может быть измерено
                // на данном канале (следует задавать только для особых физических каналов)
                "max_voltage" : 15,
        },
        {
                "id" : "A2",
                "averaging_window": 1,
                "channel_number" : 2,
                "voltage_multiplier" : 8.3917,
                "decimal_places" : 2
        },
        {
                "id" : "A3",
                "averaging_window": 1,
                "channel_number" : 1,
                "voltage_multiplier" : 8.3917,
                "decimal_places" : 2
        },
        {
                "id" : "A4",
                "averaging_window": 1,
                "channel_number" : 3,
                "voltage_multiplier" : 8.3917,
                "decimal_places" : 2
        },
		{
                "id" : "Vin",
                "averaging_window": 1,
                "channel_number" : 8,
                "voltage_multiplier" : 8.3917,
                "decimal_places" : 2
        },
		{
                "id" : "5Vout",
                "averaging_window": 1,
                "channel_number" : 5,
                "voltage_multiplier" : 3.75,
                "decimal_places" : 2
        }
    ]
}
```

Драйвер ADC считывает с файла in_voltageНОМЕРКАНАЛА_scale_available (если он существует) возможные значения scale, максимальное из них записывается в файл
in_voltageНомерКанала_scale. Множитель scale отвечает за перевод значений считанных с in_voltageНомерКанала_raw в вольты, соответственно чем больше scale,
тем большее напряжение можно измерить на данном физическом канале.








