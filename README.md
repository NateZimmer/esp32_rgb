# RGB LED Site Demo


## Usage

* Configure the project using `make menuconfig` and goto `Example Configuration` ->
    1. WIFI SSID: WIFI network to which your PC is also connected to.
    2. WIFI Password: WIFI password

* In order to test the file server demo :
    1. compile and burn the firmware `make flash`
    2. run `make monitor` and note down the IP assigned to your ESP module. The default port is 80
    3. test the example interactively on a web browser (assuming IP is 192.168.43.130):
    
    1. open path `http://192.168.43.130/`

## Note

Browsers often send large header fields when an HTML form is submit. Therefore, for the purpose of this example, `HTTPD_MAX_REQ_HDR_LEN` has been increased to 1024 in `sdkconfig.defaults`. User can adjust this value as per their requirement, keeping in mind the memory constraint of the hardware in use.
