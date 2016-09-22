

#define MDP_Bar_PROPS\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(rotation)\
	_PROP_X_(flip)\


#define ISP_Foo_PROPS\
	_PROP_X_(color_format)\
	_PROP_X_(cq)\
	_PROP_X_(pass2)\

/*
 * ISP Path
 */
/* ISP_Pass1_Sensor0_Props = ISP_Pass1_Sensor1_Props */
#define ISP_Pass1_Sensor0_Props\
	_PROP_X_(reg_CTL_EN_P1)\
	_PROP_X_(reg_CTL_EN_P1_DMA)\
	_PROP_X_(reg_RRZ_CTL)\
	_PROP_X_(rrz_src_w)\
	_PROP_X_(rrz_src_h)\
	_PROP_X_(rrz_dst_w)\
	_PROP_X_(rrz_dst_h)\
	_PROP_X_(rrz_hori_step)\
	_PROP_X_(rrz_vert_step)\



#define ISP_Pass1_Sensor1_Props\
	_PROP_X_(reg_CTL_EN_P1)\
	_PROP_X_(reg_CTL_EN_P1_DMA)\
	_PROP_X_(reg_RRZ_CTL)\
	_PROP_X_(rrz_src_w)\
	_PROP_X_(rrz_src_h)\
	_PROP_X_(rrz_dst_w)\
	_PROP_X_(rrz_dst_h)\
	_PROP_X_(rrz_hori_step)\
	_PROP_X_(rrz_vert_step)\


/*
 * MDP Path
 */
#define ISP_Pass2_DIP_Props\
	_PROP_X_(reg_CTL_EN_P2)\
	_PROP_X_(reg_CTL_EN_P2_DMA)\
	_PROP_X_(reg_CRZ_CONTROL)\
	_PROP_X_(crz_src_w)\
	_PROP_X_(crz_src_h)\
	_PROP_X_(crz_dst_w)\
	_PROP_X_(crz_dst_h)\
	_PROP_X_(crz_hori_step)\
	_PROP_X_(crz_vert_step)\




/* ISP_Pass2_MDP_Props = MDP_RDMA0_Path_Props */
#define ISP_Pass2_MDP_Props \
	_PROP_X_(thread_no)\
	_PROP_X_(task_handle)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(src_format)\
	_PROP_X_(rdma_blockmode)\
	_PROP_X_(rdma_colortrans)\
	_PROP_X_(rz_en_0)\
	_PROP_X_(rz_w_in_0)\
	_PROP_X_(rz_h_in_0)\
	_PROP_X_(rz_w_out_0)\
	_PROP_X_(rz_h_out_0)\
	_PROP_X_(reg0_RSZ_CONTROL)\
	_PROP_X_(rz_en_1)\
	_PROP_X_(rz_w_in_1)\
	_PROP_X_(rz_h_in_1)\
	_PROP_X_(rz_w_out_1)\
	_PROP_X_(rz_h_out_1)\
	_PROP_X_(reg1_RSZ_CONTROL)\
	_PROP_X_(tds_en)\
	_PROP_X_(tds_bypass_mid)\
	_PROP_X_(tds_bypass_high)\
	_PROP_X_(tds_bypass_adap_luma)\
	_PROP_X_(tds_w_in)\
	_PROP_X_(tds_h_in)\
	_PROP_X_(color_bypass_all)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(dst_w_0)\
	_PROP_X_(dst_h_0)\
	_PROP_X_(dst_format_0)\
	_PROP_X_(dst_rotate_0)\
	_PROP_X_(dst_rotate_en_0)\
	_PROP_X_(dst_flip_0)\
	_PROP_X_(wdma_ct_en)\
	_PROP_X_(dst_w_1)\
	_PROP_X_(dst_h_1)\
	_PROP_X_(dst_format_1)\
	_PROP_X_(dst_rotate_1)\
	_PROP_X_(dst_rotate_en_1)\
	_PROP_X_(dst_flip_1)\
	_PROP_X_(wrot_mat_en)\




#define MDP_RDMA0_Path_Props \
	_PROP_X_(thread_no)\
	_PROP_X_(task_handle)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(src_format)\
	_PROP_X_(rdma_blockmode)\
	_PROP_X_(rdma_colortrans)\
	_PROP_X_(rz_en_0)\
	_PROP_X_(rz_w_in_0)\
	_PROP_X_(rz_h_in_0)\
	_PROP_X_(rz_w_out_0)\
	_PROP_X_(rz_h_out_0)\
	_PROP_X_(reg0_RSZ_CONTROL)\
	_PROP_X_(rz_en_1)\
	_PROP_X_(rz_w_in_1)\
	_PROP_X_(rz_h_in_1)\
	_PROP_X_(rz_w_out_1)\
	_PROP_X_(rz_h_out_1)\
	_PROP_X_(reg1_RSZ_CONTROL)\
	_PROP_X_(tds_en)\
	_PROP_X_(tds_bypass_mid)\
	_PROP_X_(tds_bypass_high)\
	_PROP_X_(tds_bypass_adap_luma)\
	_PROP_X_(tds_w_in)\
	_PROP_X_(tds_h_in)\
	_PROP_X_(color_bypass_all)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(dst_w_0)\
	_PROP_X_(dst_h_0)\
	_PROP_X_(dst_format_0)\
	_PROP_X_(dst_rotate_0)\
	_PROP_X_(dst_rotate_en_0)\
	_PROP_X_(dst_flip_0)\
	_PROP_X_(wdma_ct_en)\
	_PROP_X_(dst_w_1)\
	_PROP_X_(dst_h_1)\
	_PROP_X_(dst_format_1)\
	_PROP_X_(dst_rotate_1)\
	_PROP_X_(dst_rotate_en_1)\
	_PROP_X_(dst_flip_1)\
	_PROP_X_(wrot_mat_en)\



/*
 * Display Path
 */
/* DISP_UNKNOWN_Path_Props = DISP_OVL0_Path_Props = DISP_RDMA0_Path_Props = DISP_OVL1_4L_Path_Props
 * = DISP_RDMA1_Path_Props*/
#define DISP_UNKNOWN_Path_Props\
	_PROP_X_(mutex_id)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in_0)\
	_PROP_X_(addr_in_1)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(reg0_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg0_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg1_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg0_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg0_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg1_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg1_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg_DSI_START)\
	_PROP_X_(reg_DSI_MODE_CTRL)\
	_PROP_X_(reg_DSI_TXRX_CTRL)\
	_PROP_X_(reg_DSI_VSA_NL)\
	_PROP_X_(reg_DSI_VBP_NL)\
	_PROP_X_(reg_DSI_VFP_NL)\
	_PROP_X_(reg_DSI_VACT_NL)\
	_PROP_X_(reg_DSI_HSA_WC)\
	_PROP_X_(reg_DSI_HBP_WC)\
	_PROP_X_(reg_DSI_HFP_WC)\
	_PROP_X_(reg_DSI_BLLP_WC)\
	_PROP_X_(reg_DSI_PHY_LCCON)\
	_PROP_X_(reg_DSI_PHY_LD0CON)\
	_PROP_X_(reg_DISP_REG_DPI_EN)\
	_PROP_X_(reg_DISP_REG_DPI_SIZE)\


#define DISP_OVL0_Path_Props\
	_PROP_X_(mutex_id)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in_0)\
	_PROP_X_(addr_in_1)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(reg0_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg0_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg1_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg0_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg0_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg1_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg1_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg_DSI_START)\
	_PROP_X_(reg_DSI_MODE_CTRL)\
	_PROP_X_(reg_DSI_TXRX_CTRL)\
	_PROP_X_(reg_DSI_VSA_NL)\
	_PROP_X_(reg_DSI_VBP_NL)\
	_PROP_X_(reg_DSI_VFP_NL)\
	_PROP_X_(reg_DSI_VACT_NL)\
	_PROP_X_(reg_DSI_HSA_WC)\
	_PROP_X_(reg_DSI_HBP_WC)\
	_PROP_X_(reg_DSI_HFP_WC)\
	_PROP_X_(reg_DSI_BLLP_WC)\
	_PROP_X_(reg_DSI_PHY_LCCON)\
	_PROP_X_(reg_DSI_PHY_LD0CON)\
	_PROP_X_(reg_DISP_REG_DPI_EN)\
	_PROP_X_(reg_DISP_REG_DPI_SIZE)\


#define DISP_RDMA0_Path_Props\
	_PROP_X_(mutex_id)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in_0)\
	_PROP_X_(addr_in_1)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(reg0_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg0_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg1_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg0_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg0_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg1_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg1_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg_DSI_START)\
	_PROP_X_(reg_DSI_MODE_CTRL)\
	_PROP_X_(reg_DSI_TXRX_CTRL)\
	_PROP_X_(reg_DSI_VSA_NL)\
	_PROP_X_(reg_DSI_VBP_NL)\
	_PROP_X_(reg_DSI_VFP_NL)\
	_PROP_X_(reg_DSI_VACT_NL)\
	_PROP_X_(reg_DSI_HSA_WC)\
	_PROP_X_(reg_DSI_HBP_WC)\
	_PROP_X_(reg_DSI_HFP_WC)\
	_PROP_X_(reg_DSI_BLLP_WC)\
	_PROP_X_(reg_DSI_PHY_LCCON)\
	_PROP_X_(reg_DSI_PHY_LD0CON)\
	_PROP_X_(reg_DISP_REG_DPI_EN)\
	_PROP_X_(reg_DISP_REG_DPI_SIZE)\


#define DISP_OVL1_4L_Path_Props\
	_PROP_X_(mutex_id)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in_0)\
	_PROP_X_(addr_in_1)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(reg0_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg0_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg1_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg0_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg0_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg1_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg1_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg_DSI_START)\
	_PROP_X_(reg_DSI_MODE_CTRL)\
	_PROP_X_(reg_DSI_TXRX_CTRL)\
	_PROP_X_(reg_DSI_VSA_NL)\
	_PROP_X_(reg_DSI_VBP_NL)\
	_PROP_X_(reg_DSI_VFP_NL)\
	_PROP_X_(reg_DSI_VACT_NL)\
	_PROP_X_(reg_DSI_HSA_WC)\
	_PROP_X_(reg_DSI_HBP_WC)\
	_PROP_X_(reg_DSI_HFP_WC)\
	_PROP_X_(reg_DSI_BLLP_WC)\
	_PROP_X_(reg_DSI_PHY_LCCON)\
	_PROP_X_(reg_DSI_PHY_LD0CON)\
	_PROP_X_(reg_DISP_REG_DPI_EN)\
	_PROP_X_(reg_DISP_REG_DPI_SIZE)\


#define DISP_RDMA1_Path_Props\
	_PROP_X_(mutex_id)\
	_PROP_X_(engine_flag)\
	_PROP_X_(active_flag)\
	_PROP_X_(addr_in_0)\
	_PROP_X_(addr_in_1)\
	_PROP_X_(addr_out_0)\
	_PROP_X_(addr_out_1)\
	_PROP_X_(reg_DISP_COLOR_CFG_MAIN)\
	_PROP_X_(reg0_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg0_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg0_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_GLOBAL_CON)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_0)\
	_PROP_X_(reg1_DISP_REG_RDMA_SIZE_CON_1)\
	_PROP_X_(reg1_DISP_REG_RDMA_MEM_CON)\
	_PROP_X_(reg0_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg0_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg1_DISP_REG_WDMA_CFG)\
	_PROP_X_(reg1_DISP_REG_WDMA_CLIP_SIZE)\
	_PROP_X_(reg_DSI_START)\
	_PROP_X_(reg_DSI_MODE_CTRL)\
	_PROP_X_(reg_DSI_TXRX_CTRL)\
	_PROP_X_(reg_DSI_VSA_NL)\
	_PROP_X_(reg_DSI_VBP_NL)\
	_PROP_X_(reg_DSI_VFP_NL)\
	_PROP_X_(reg_DSI_VACT_NL)\
	_PROP_X_(reg_DSI_HSA_WC)\
	_PROP_X_(reg_DSI_HBP_WC)\
	_PROP_X_(reg_DSI_HFP_WC)\
	_PROP_X_(reg_DSI_BLLP_WC)\
	_PROP_X_(reg_DSI_PHY_LCCON)\
	_PROP_X_(reg_DSI_PHY_LD0CON)\
	_PROP_X_(reg_DISP_REG_DPI_EN)\
	_PROP_X_(reg_DISP_REG_DPI_SIZE)\


/*
 * OVL Layers
 */
/* OVL_LAYER_L0_Props = OVL_LAYER_L1_Props = OVL_LAYER_L2_Props */
#define OVL_LAYER_L0_Props\
	_PROP_X_(layer)\
	_PROP_X_(layer_en)\
	_PROP_X_(fmt)\
	_PROP_X_(addr)\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(src_pitch)\
	_PROP_X_(bpp)\
	_PROP_X_(gpu_mode)\
	_PROP_X_(adobe_mode)\
	_PROP_X_(ovl_gamma_out)\
	_PROP_X_(alpha)\

#define OVL_LAYER_L1_Props\
	_PROP_X_(layer)\
	_PROP_X_(layer_en)\
	_PROP_X_(fmt)\
	_PROP_X_(addr)\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(src_pitch)\
	_PROP_X_(bpp)\
	_PROP_X_(gpu_mode)\
	_PROP_X_(adobe_mode)\
	_PROP_X_(ovl_gamma_out)\
	_PROP_X_(alpha)\

#define OVL_LAYER_L2_Props\
	_PROP_X_(layer)\
	_PROP_X_(layer_en)\
	_PROP_X_(fmt)\
	_PROP_X_(addr)\
	_PROP_X_(src_w)\
	_PROP_X_(src_h)\
	_PROP_X_(src_pitch)\
	_PROP_X_(bpp)\
	_PROP_X_(gpu_mode)\
	_PROP_X_(adobe_mode)\
	_PROP_X_(ovl_gamma_out)\
	_PROP_X_(alpha)\




