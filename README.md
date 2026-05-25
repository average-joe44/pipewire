# PipeWire
PipeWire is a simple handler for connection from reverse-shell and exploit.  

## ABOUT
This handler is not multi client cuz i want it simpler and easier to code.  
Only works on linux, but, if you rev-shell a windows system the ```download``` and ```upload``` functions won't work since it used a linux native tool to works.  

## USAGE
Git clone it first then cd
```
git clone https://github.com/average-joe44/PipeWire && cd PipeWire
```
Compile the source code
```
gcc handler.c -o handler
```
Give a executable permission with chmod
```
chmod +x handler
```
After all that, run it
```
./handler <port>
```
After a connection is received you should see a "**handler>>**" input in your terminal, then you can type
```
handler>> ls
handler>> cat
handler>> echo
handler>> cd
handler>> pwd
handler>> ifconfig
...etc
```
Or you can download and upload a file
```
download <remote_file_name> <local_file_name>
```
```
upload <local_file_name> <remote_file_name>
```
- download
  > remote_file_name = what file you want to download (from target)
  > 
  > local_file_name = what name you want to save it (your machine)

- upload
  > local_file_name = what file you want to upload (your machine)
  >
  > remote_file_name = what name you want to save it (in target)

## NOTE
This is again a PoC project you can't use this for real illegal activities.  
Hope you like it :).
