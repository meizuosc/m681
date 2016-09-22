/*
 * met_udtl_traceline.h
 *
 *  Created on: 2015/5/11
 *      Author: MTK02679
 */

/* Define Traceline as below                                    */
/*                                                              */
/* MET_UDTL_TRACELINE( PAGE_NAME,  TRACELINE_NAME, PROPS_NAME ) */
/*                                                              */


MET_UDTL_TRACELINE(PAGE_NAME_Dummy, MDP__ModuleBar, MDP_Bar_PROPS)
MET_UDTL_TRACELINE(PAGE_NAME_Dummy, ISP__ModuleFoo, ISP_Foo_PROPS)


/*
 * ISP Path
 */
MET_UDTL_TRACELINE(MMSYS, ISP__Pass1_Sensor0, ISP_Pass1_Sensor0_Props)
MET_UDTL_TRACELINE(MMSYS, ISP__Pass1_Sensor1, ISP_Pass1_Sensor1_Props)

/*
 * MDP Path
 */
MET_UDTL_TRACELINE(MMSYS, ISP__Pass2_DIP,	ISP_Pass2_DIP_Props)	/*MDP ISP Pass 2 DIP*/
																	/*(This traceline's props is diff w/ MDP's*/

MET_UDTL_TRACELINE(MMSYS, ISP__Pass2_MDP,	ISP_Pass2_MDP_Props)	/*MDP ISP Pass 2 MDP*/
MET_UDTL_TRACELINE(MMSYS, MDP__RDMA0_Path,	MDP_RDMA0_Path_Props)	/*MDP RDMA Path*/


/*
 * Display Path
 */
MET_UDTL_TRACELINE(MMSYS, DISP__UNKNOWN_Path, DISP_UNKNOWN_Path_Props)

MET_UDTL_TRACELINE(MMSYS, DISP__OVL0_Path, DISP_OVL0_Path_Props)
MET_UDTL_TRACELINE(MMSYS, DISP__RDMA0_Path, DISP_RDMA0_Path_Props)
MET_UDTL_TRACELINE(MMSYS, DISP__OVL1_4L_Path, DISP_OVL1_4L_Path_Props)
MET_UDTL_TRACELINE(MMSYS, DISP__RDMA1_Path, DISP_RDMA1_Path_Props)

/*
 * OVL Layers (Special case for met)
 */
MET_UDTL_TRACELINE(MMSYS, OVL_LAYERS_L0__LAYER, OVL_LAYER_L0_Props)
MET_UDTL_TRACELINE(MMSYS, OVL_LAYERS_L1__LAYER, OVL_LAYER_L1_Props)
MET_UDTL_TRACELINE(MMSYS, OVL_LAYERS_L2__LAYER, OVL_LAYER_L2_Props)
