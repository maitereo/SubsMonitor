//配网页面代码
const char *page_html = "\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n\
  <title>Document</title>\r\n\
</head>\r\n\
<body>\r\n\
  <h1>ESP8266配置页</h1>\r\n\
  <form name='input' action='/' method='POST'>\r\n\
    WiFi名称:\r\n\
    <input type='text' name='ssid'><br>\r\n\
    WiFi密码:\r\n\
    <input type='password' name='password'><br>\r\n\
    BiliBili ID:\r\n\
    <input type='BilibiliID' name='BilibiliID'><br>\r\n\
    <input type='submit' value='提交'>\r\n\
    <br><br>\r\n\
    <h2>加油！祝你早日突破10w粉！—— YShan</h2>\r\n\
    <a href='https://github.com/maitereo'> Maitereo </a>\r\n\
  </form>\r\n\
</body>\r\n\
</html>\r\n\
";