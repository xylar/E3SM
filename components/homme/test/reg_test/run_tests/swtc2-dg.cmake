###############################################################
# RKSSP default benchmark (used to check nothing is broken)
###############################################################
#
# Discontinous Galerkin swtc2
# NE=6, dt=30, nu=0, filter_freq=1, NP=6
#
###############################################################

# The name of this test (should be the basename of this file)
SET(TEST_NAME swtc2-dg)
# The type of run (preqx,sweqx,swdgx,etc.)
SET(TEST_TYPE swdgx)
# The specifically compiled executable that this test uses
SET(EXEC_NAME swtc-dgA)

SET(NUM_CPUS 16)

SET(NAMELIST_FILES ${HOMME_ROOT}/test/reg_test/namelists/${TEST_NAME}.nl)

SET(NC_OUTPUT_FILES swtc21.nc)