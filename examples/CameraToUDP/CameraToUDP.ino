// Spresenseカメラの映像をUDP送信します。
// 受信側PCでは、 https://github.com/crane-elec/UDPJPG のテストソフト UDP_JPG.exe を起動しておきます。
// UDPポートは、50001 固定です。
// UDPを受信してカメラ画像が表示されます。
// 本サンプルの、W5500とPCのIPアドレスは環境に応じて変更してください。
// 一般的にはIPアドレスの上位3つは、ルーターのIPアドレスと同じにします。
// ルーターIP 192.168.1.1 なら192.168.1.XXX
// ルーターIP 192.168.100.1 なら192.168.100.XXX
// など。
// 


#include <EthernetSP.h>
#include <EthernetUdp.h>
#include <Camera.h>

#define BAUDRATE (115200)
#define RX_BUFFER_SIZE (256)
#define UDP_TX_SIZE (1472) //Sending data size of 1 frame. (MTU = 1500, IPheader 20, UDP header 8 )

// UDP settings
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 100, 234); // W5500 IP
unsigned int localPort = 50000;    // W5500 port

IPAddress ip_remote(192,168,100,49); // Your PC IP
unsigned int Port_remote = 50001;     // Your PC receive port

char RxBuffer[RX_BUFFER_SIZE];

EthernetUDP Udp;

void printError(enum CamErr err)
{
    Serial.print("Error: ");
    switch (err)
    {
    case CAM_ERR_NO_DEVICE:
        Serial.println("No Device");
        break;
    case CAM_ERR_ILLEGAL_DEVERR:
        Serial.println("Illegal device error");
        break;
    case CAM_ERR_ALREADY_INITIALIZED:
        Serial.println("Already initialized");
        break;
    case CAM_ERR_NOT_INITIALIZED:
        Serial.println("Not initialized");
        break;
    case CAM_ERR_NOT_STILL_INITIALIZED:
        Serial.println("Still picture not initialized");
        break;
    case CAM_ERR_CANT_CREATE_THREAD:
        Serial.println("Failed to create thread");
        break;
    case CAM_ERR_INVALID_PARAM:
        Serial.println("Invalid parameter");
        break;
    case CAM_ERR_NO_MEMORY:
        Serial.println("No memory");
        break;
    case CAM_ERR_USR_INUSED:
        Serial.println("Buffer already in use");
        break;
    case CAM_ERR_NOT_PERMITTED:
        Serial.println("Operation not permitted");
        break;
    default:
        break;
    }
}

void RebootUDP()
{
    Udp.stop();
    
    // Reboot W5500
    W5500ETH_reset(SJ1_12);
    Ethernet.begin(mac, ip);
    while (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.print(".");
        delay(100);
        // sleep(1);
    }
    Ethernet.maintain();

    Udp.begin(localPort);
}

void setup()
{
    CamErr err;

    Serial.begin(BAUDRATE);
    while (!Serial)
    {
        ;
    }

    // ====================================================================
    // W5500-Ether add-on for Spresense

    // Jumper setting is SJ1=12, use D21/EMMC_DATA3
    W5500ETH_reset(SJ1_12);

    // Jumper setting is SJ1=23, use D26/I2S_BCK
    // W5500ETH_reset(SJ1_23);

    // When Jumper setting is SJ2=23, init() is not need.

    // Jumper setting is SJ2=12, use D19/I2S_DIN
    // Ethernet.init(SJ2_12);
    // ====================================================================

    // start the Ethernet
    Ethernet.begin(mac, ip);
    Ethernet.setRetransmissionCount(1);
    Ethernet.setRetransmissionTimeout(100);
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        Serial.println("W5500 was not found.  Sorry, can't run without hardware. :(");
        while (true)
        {
            delay(1); // do nothing, no point running without Ethernet hardware
        }
    }
    while (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("Ethernet cable is not connected.");
        sleep(1);
    }

    // start UDP
    Udp.begin(localPort);

    // カメラの初期化
    Serial.println("Camera initialization...");
    err = theCamera.begin();
    if (err != CAM_ERR_SUCCESS)
    {
        printError(err);
        while (1)
            ;
    }

    // 画像フォーマットの設定
    err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_AUTO);
    err = theCamera.setStillPictureImageFormat(CAM_IMGSIZE_VGA_H, CAM_IMGSIZE_VGA_V, CAM_IMAGE_PIX_FMT_JPG);
    // err = theCamera.setStillPictureImageFormat(CAM_IMGSIZE_QVGA_H, CAM_IMGSIZE_QVGA_V, CAM_IMAGE_PIX_FMT_JPG);
    // err = theCamera.setStillPictureImageFormat(CAM_IMGSIZE_QQVGA_H, CAM_IMGSIZE_QQVGA_V, CAM_IMAGE_PIX_FMT_JPG);
}

void loop()
{
    CamImage img = theCamera.takePicture();
    delay(30);//PC側サンプルアプリ(UDPJPG.exe)の、フレーム間30msec以上あける仕組み、に対応させている。

    bool tx_result;
    uint16_t tx_size;

    if (img.isAvailable())
    {
        uint8_t *imgBuff = (uint8_t *)img.getImgBuff();

        int width = img.getWidth();
        int height = img.getHeight();
        int siz = img.getImgSize();
        int written = 0;
        tx_result = true;
        // Serial.print("jpg data size: ");
        // Serial.print(siz);
        // Serial.print(" -> packets [");

        // jpgデータをUDP送信 UDP_TX_SIZE単位で送る
        while((0 < siz) && tx_result == true)
        {
            // 受信データがある場合、読み捨ててRXバッファをケアしておく
            if(0 < Udp.parsePacket())
            {
                Udp.read(RxBuffer, RX_BUFFER_SIZE);
            }

            // 1フレームで送るデータ量を決定
            if(UDP_TX_SIZE < siz)
            {
                tx_size = UDP_TX_SIZE;//1フレーム分 満タン
            }
            else
            {
                tx_size = siz;//最後の端数
            }
            Udp.beginPacket(ip_remote, Port_remote);
            written = Udp.write(imgBuff, tx_size);
            tx_size = written;//送るデータ量を実際に送信バッファに書き込みできた容量へ変更
            tx_result = Udp.endPacket();

            // Serial.print(tx_size);
            // Serial.print(" ");
            imgBuff += tx_size;
            siz-=tx_size;

            //簡易的なインターフレームギャップ
            delay(1);

        }
        // UDP transfer error or/and no response from W5500.
        if(tx_result == false)
        {
            Serial.print("UDP error.");
            RebootUDP();
            Serial.println("Re-connected.");
        }

        // Serial.println("]");
    }
    else
    {
        Serial.println("Failed to capture image.");
        theCamera.end();
        sleep(1);
        theCamera.begin();
        theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_AUTO);
        theCamera.setStillPictureImageFormat(CAM_IMGSIZE_VGA_H, CAM_IMGSIZE_VGA_V, CAM_IMAGE_PIX_FMT_JPG);
    }

}
