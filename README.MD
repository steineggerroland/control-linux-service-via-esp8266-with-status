

## Server

```bash
sudo adduser <user>
sudo apt install python3 python3-flask
sudo ln -s /home/<user>/api_server.service /etc/systemd/system/api_server.service
sudo systemctl start api_server.service
sudo usermod -a -G sudo <user>
sudo visudo
# ADD <user> ALL=(ALL) NOPASSWD: /bin/systemctl start the.service, /bin/systemctl stop the.service
```

Possible java service

```
[Unit]
Description=The java service
After=network.target

[Service]
SuccessExitStatus=143
Type=simple

ExecStart=/usr/bin/java -jar /home/<user>/the.jar
ExecStop=/bin/kill -15 $MAINPID
WorkingDirectory=/home/<user>
User=<user>

[Install]
WantedBy=multi-user.target
```
