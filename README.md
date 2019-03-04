# project-lava
Optimized Packet Passing for Virtual Networks

  - mainScript.sh <flags>
    * -t Will run test
    * -w will push test csv files to the git wiki repo
    * currently they need to be run indiviudally.
  
  - testScript.sh
    * no flags this script will run framework n:m up to n = 8, m = 8
    * Called by mainScript.sh
    * Should be a sudo call since we work with threads.
