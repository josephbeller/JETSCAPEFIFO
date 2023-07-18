# JETSCAPE FIFO

## Installation
These instructions assume you've already installed docker on your computer and added yourself to a group with root privileges per the
JETSCAPE install directions. If you haven't here are the steps:
### Step 0: 
Skip step 0 if you've already installed docker on your computer. 
 #### macOS

1. Install Docker Desktop for Mac: https://docs.docker.com/docker-for-mac/install/
2. Open Docker, go to Preferences (gear icon) --> Resources --> Advanced and
    - (i) Set CPUs to max that your computer has (`sysctl -n hw.ncpu`),
    - (ii) Set memory to what you are willing to give Docker.

 #### linux/windows

1. Install Docker: https://docs.docker.com/install/
2. Allow your user to run docker (requires admin privileges):
    ```
    sudo groupadd docker
    sudo usermod -aG docker $USER
    ```
    Log out and log back in.

### Step 1: Create  New Docker Container
 Create a new container using a JETSCAPE image that has RIVET dependencies per-installed.
 #### macOS
```
cd ~
mkdir jetscape-rivet-docker
cd jetscape-rivet-docker
docker run -it -v ~/jetscape-rivet-docker:/home/jetscape-rivet-user --name 
    myJetscapeRivet -p 8888:8888 tmengel/jetscaperivet:latest
```

 #### linux
```
docker run -it -v ~/jetscape-docker:/home/jetscape-rivet-user --name myJetscapeRivet -p 8888:8888 --user $(id -u):$(id -g) 
    tmengel/jetscaperivet:latest
```

For **Windows**, please follow the analogous instructions: https://docs.docker.com/install/

Please note that if you have an older OS, you may need to download an older version of docker.]

### Step 1-b 
Make sure that you are inside the correct directory in the docker.  
```
cd /home/jetscape-rivet-user
```

### Step 2: Clone JETSCAPEFIFO
Now clone a modified version of JETSCAPE which contains the necessary HepMC3 Fifo writing modules. The only changes are an addition module for HepMC3-fifo output and a directory called **JetScapeWriterHepMCfifo** with an example XML file to run the new module.

```
 git clone https://github.com/tmengel/JETSCAPEFIFO.git
 ```

### Step 3: Bootstrap RIVET
Within the docker container you should see **I have no name!@...:~**. This shows that you are in the container. **Make sure you see this before continuing**.

#### linux/windows
```
 cd /JETSCAPEFIFO/external_packages
./get_rivet.sh
```
#### mac
```
 cd JETSCAPEFIFO/external_packages
./get_rivet.sh mac
```
This will bootstrap rivet in a new directory `~/RIVET`. This process will take about 15-20 minutes. 

### Step 4: Build JETSCAPE
roceed to build this version of JETSCAPE the same way you would for the original version [Installation Instructions](https://github.com/JETSCAPE/JETSCAPE/wiki/Doc.Installation).

```
cd JETSCAPEFIFO
mkdir build
cd build
cmake .. -DCMAKE_CXX_STANDARD=14
make -j4
```
## Running JETSCAPE and RIVET
To run JETSCAPE with RIVET using the fifo-hepmc writer use the `jetscape_fifo.xml` located in the `config` directory.
Make sure the output file matches the file piped into RIVET. Execute the `runJetscape` executable located in the JETSCAPE build directory in the background and send the output to RIVET. In the `~/JETSCAPEFIFO/build` directory run:
```
./runJetscape ../config/jetscape_fifo.xml & 
    rivet --analysis=<ANALYSIS NAME> --ignore-beams -o test.yoda myfifo.hepmc
```
The `/config/jetscape_fifo.xml` is an example of how to use the **JetScapeWriterHepMCfifo** module.


## Troubleshooting
For questions email tmengel@vols.utk.edu
