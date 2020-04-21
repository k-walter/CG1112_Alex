# Installation
//test
- Take note of master (PC) and slave (Alex) IP Address
```console
me@master:~$ hostname -I
```

- Add PC and Pi's host names to /etc/hosts
```console
me@master:~$ sudo echo -e "$(master_ip) master\n$(slave_ip) slave" >> /etc/hosts
pi@slave:~$ sudo echo -e "$(master_ip) master\n$(slave_ip) slave" >> /etc/hosts
```

- Setup SSH public keys for ROS
```console
me@master:~$ ssh-keygen -t rsa
Generating public/private rsa key pair.
Enter file in which to save the key (/home/me/.ssh/id_rsa):
Enter passphrase (empty for no passphrase): 
Enter same passphrase again: 
Your identification has been saved in /home/me/.ssh/id_rsa.
Your public key has been saved in /home/me/.ssh/id_rsa.pub.
The key fingerprint is:
SHA256:Up6KjbnEV4Hgfo75YM393QdQsK3Z0aTNBz0DoirrW+c me@master
The key's randomart image is:
+---[RSA 2048]----+
|    .      ..oo..|
|   . . .  . .o.X.|
|    . . o.  ..+ B|
|   .   o.o  .+ ..|
|    ..o.S   o..  |
|   . %o=      .  |
|    @.B...     . |
|   o.=. o. . .  .|
|    .oo  E. . .. |
+----[SHA256]-----+
me@master:~$ ssh-copy-id pi@slave
```
- Install and make rplidar_ros on both machines as per studio. Follow Studio 11 pg 2-4 to reduce power usage on RPi.

- Export ROS settings in respective setup.bash
```console
me@master:~$ sudo echo -e "export ROS_MASTER_URI = http://$(master_ip):11311/\nexport ROS_IP = $(master_ip)\nexport ROS_HOSTNAME = master" >> setup.bash
pi@slave:~$ sudo echo -e "export ROS_MASTER_URI = http://$(master_ip):11311/\nexport ROS_IP = $(slave_ip)\nexport ROS_HOSTNAME = slave" >> setup.bash
```

- In master, replace view_slam.launch in "slam\src\rplidar_ros\launch\" with this repo's. Edit fields labelled "HERE" in view_slam.launch

- In slave, copy W8S1

# Getting Started

## Movement Terminal
SSH to slave and run move.bash in W8S1
```console
me@master:~$ ssh pi@slave
Linux raspberrypi 4.14.98-v7+ #1200 SMP Tue Feb 12 20:27:48 GMT 2019 armv7l

The programs included with the Debian GNU/Linux system are free software;
the exact distribution terms for each program are described in the
individual files in /usr/share/doc/*/copyright.

Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
permitted by applicable law.
Last login:
pi@slave:~$ cd ~/W8S1
pi@slave:~/W8S1$ . move.bash
```

## Hector SLAM
- Run view_slam.launch, which should open rviz.
- If there are errors, disable firewall and check dependencies.
```console
pi@slave:~$ . slam/devel/setup.bash
pi@slave:~$ roslaunch rplidar_ros view_slam.launch
... logging to
Checking log directory for disk usage. This may take a while.
Press Ctrl-C to interrupt
Done checking log file disk usage. Usage is <1GB.

started roslaunch server http://master:6422/
remote[slave-0] starting roslaunch
remote[slave-0]: creating ssh connection to slave:22, user[slave]
launching remote roslaunch child with command: [env ... ... roslaunch -c slave-0 -u http://master:6422/ --run_id ddb0a6c8-8310-11ea-9298-3cf011fd2342]
remote[slave-0]: ssh connection created

SUMMARY
========
PARAMETERS
 * /hector_height_mapping/advertise_map_service: True
 * /hector_height_mapping/base_frame: base_link
 * /hector_height_mapping/map_pub_period: 0.5 
 * /hector_height_mapping/map_resolution: 0.05
 * /hector_height_mapping/map_size: 1024
 * /hector_height_mapping/map_start_x: 0.5
 * /hector_height_mapping/map_start_y: 0.5
 * /hector_height_mapping/map_update_angle_thresh: 0.1
 * /hector_height_mapping/map_update_distance_thresh: 0.02
 * /hector_height_mapping/map_with_known_poses: False
 * /hector_height_mapping/odom_frame: base_link
 * /hector_height_mapping/output_timing: False
 * /hector_height_mapping/pub_map_odom_transform: True
 * /hector_height_mapping/scan_topic: scan
 * /hector_height_mapping/update_factor_free: 0.45
 * /hector_height_mapping/use_tf_pose_start_estimate: False
 * /hector_height_mapping/use_tf_scan_transformation: True
 * /rosdistro: melodic
 * /rosversion: 1.14.5
 * /rplidarNode/angle_compensate: True
 * /rplidarNode/frame_id: laser
 * /rplidarNode/inverted: False
 * /rplidarNode/serial_baudrate: 115200
 * /rplidarNode/serial_port: /dev/ttyUSB0
 * /use_sim_time: False

MACHINES
 * master
 * slave

NODES
  /
    hector_height_mapping (hector_mapping/hector_mapping)
    link1_broadcaster (tf/static_transform_publisher)
    rplidarNode (rplidar_ros/rplidarNode)
    rviz (rviz/rviz)

ROS_MASTER_URI=http://...:11311

process[rviz-1]: started with pid [4498]
[slave-0]: launching nodes...
[slave-0]: ROS_MASTER_URI=http://...:11311/
[slave-0]: process[rplidarNode-1]: started with pid [12126]
[slave-0]: process[link1_broadcaster-2]: started with pid [12127]
[slave-0]: process[hector_height_mapping-3]: started with pid [12131]
[slave-0]: ... done launching nodes
```
- In RViz, select Displays/Map/Unreliable
- For convenience, close sidebars and File/Save config
