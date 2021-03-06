#!/bin/csh
#set DIR=/Users/sievers/act/mapper/
#set DIR=/mnt/scratch-3week/sievers/act/mapper/
#set DIR=/cita/d/raid-nolta/act/data/season1/


set DIR=/cita/d/raid-sievers/sievers/act/mapper/
set DIR=/cita/d/raid-nolta/act/data/season1/

mpirun N ./ninkasi << EOF

@tol 1e-4
@maxiter 100 
#@input ${DIR}/src/simple_cmb_map_seed_1.dat 
@output ./sim_equitorial_wnoise.map  
@data [${DIR}/1197424216.1197424241 ${DIR}/1197434953.1197434974]
#@data [${DIR}/real_data/1193729422.1193729537]
    
@temp sim_equitorial_wnoise
@pointing_offsets /cita/d/raid-sievers/sievers/act/ninkasi/from_ishmael/pointing_offset_mbac145.txt
#@deglitch

#@altaz_file altaz_ctime_equitorial_small.txt
@altaz_file altaz_ctime_equitorial.txt

@precon
@pixsize 0.5

#@use_cols [8 9 11 12 13 15 16 18 19 20 21 23]
#@use_rows [19] # - bad detector



#@no_noise
@blank
#@mean
#@maxtod 2
@add_noise
#@sim
#@quit
#finished 
EOF


