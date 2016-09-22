#define SENSE_NUM_Y 24
#define DRIVE_NUM_Y 14
#define KEY_NUM_Y 0
#define KEY_LINE_Y 0
#define GR_NUM_Y 0
#define CSUB_REF_Y 3
#define SENSE_MUTUAL_SCAN_NUM_Y 2
#define MUTUAL_KEY_Y 0
#define PATTERN_TYPE_Y 4


#define SHORT_N1_TEST_NUMBER_Y 12
#define SHORT_N2_TEST_NUMBER_Y 12
#define SHORT_S1_TEST_NUMBER_Y 7
#define SHORT_S2_TEST_NUMBER_Y 7
#define SHORT_TEST_5_TYPE_Y 0
#define SHORT_X_TEST_NUMBER_Y  0




u16 MSG28XX_SHORT_N1_TEST_PIN_Y[SHORT_N1_TEST_NUMBER_Y]={2,10,12,14,16,18,19,21,26,28,30,33};
u16 SHORT_N1_MUX_MEM_20_3E_Y[16]={0x0100,0x0000,0x0200,0x0403,0x7605,0x0080,0x0900,0x0B0A,0x00C0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};
u16 MSG28XX_SHORT_N2_TEST_PIN_Y[SHORT_N2_TEST_NUMBER_Y]={0,3,11,13,15,17,20,25,27,29,32,34};
u16 SHORT_N2_MUX_MEM_20_3E_Y[16]={0x2001,0x0000,0x3000,0x5040,0x0060,0x0007,0x9080,0x00A0,0x0C0B,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};
u16 MSG28XX_SHORT_S1_TEST_PIN_Y[SHORT_S1_TEST_NUMBER_Y]={36,38,41,43,45,47,49};
u16 SHORT_S1_MUX_MEM_20_3E_Y[16]={0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0201,0x4030,0x6050,0x0070,0x0000,0x0000,0x0000};
u16 MSG28XX_SHORT_S2_TEST_PIN_Y[SHORT_S1_TEST_NUMBER_Y]={37,40,42,44,46,48,58};
u16 SHORT_S2_MUX_MEM_20_3E_Y[16]={0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0010,0x0302,0x0504,0x0006,0x0000,0x0700,0x0000};
u16 MSG28XX_SHORT_X_TEST_PIN_Y[SHORT_X_TEST_NUMBER_Y]={};
u16 SHORT_X_MUX_MEM_20_3E_Y[16]={};


u16 PAD_TABLE_DRIVE_Y[DRIVE_NUM_Y]={36,37,38,40,41,42,43,44,45,46,47,48,49,58};
u16 PAD_TABLE_SENSE_Y[SENSE_NUM_Y]={19,20,21,25,26,27,28,29,30,32,33,34,18,17,16,15,14,13,12,11,10,3,2,0};
u16 PAD_TABLE_GR_Y[GR_NUM_Y];

u8 KEYSEN_Y[]={0};
u8 KEYDRV_Y[]={0};
s16 GOLDEN_CH_Y[]={4719,4820,4819,4820,4832,4835,4841,4822,4849,4860,4848,4877,4899,4967,4744,4853,4857,4858,4872,4867,4876,4864,4892,4897,4879,4914,4928,5000,4739,4857,4870,4869,4882,4880,4890,4874,4904,4904,4889,4922,4938,5006,4743,4860,4873,4877,4892,4883,4895,4883,4910,4912,4893,4916,4930,5002,4746,4859,4866,4871,4887,4878,4888,4875,4901,4904,4884,4903,4915,5009,4727,4840,4850,4849,4867,4855,4866,4854,4880,4877,4860,4873,4890,4995,4727,4835,4843,4840,4865,4852,4862,4847,4869,4870,4850,4867,4887,5001,4731,4838,4852,4854,4866,4854,4866,4845,4869,4874,4854,4874,4891,5006,4724,4843,4848,4845,4853,4845,4855,4839,4864,4865,4851,4865,4894,5031,4387,4829,4843,4841,4850,4844,4847,4830,4851,4856,4844,4869,4891,5034,4741,4836,4840,4843,4854,4840,4847,4833,4853,4863,4850,4878,4899,5037,4821,4846,4847,4844,4846,4839,4843,4830,4857,4871,4854,4881,4902,5067,5219,4901,4892,4888,4867,4857,4856,4856,4853,4856,4851,4859,4846,4759,5140,4890,4878,4868,4853,4845,4846,4848,4844,4848,4843,4843,4832,4754,5109,4888,4872,4864,4858,4846,4847,4849,4843,4842,4836,4844,4833,4755,5098,4883,4877,4870,4861,4851,4856,4851,4850,4849,4845,4852,4843,4761,5104,4876,4867,4864,4858,4850,4850,4843,4845,4847,4839,4847,4834,4751,5102,4877,4869,4865,4862,4852,4851,4845,4848,4849,4841,4849,4835,4750,5106,4889,4879,4882,4879,4861,4869,4867,4867,4871,4860,4857,4844,4761,5103,4884,4880,4883,4877,4868,4868,4865,4866,4863,4847,4851,4841,4756,5108,4874,4866,4863,4862,4857,4853,4852,4850,4850,4834,4835,4827,4740,5104,4867,4857,4859,4849,4840,4838,4837,4838,4841,4819,4826,4819,4733,5119,4873,4863,4857,4846,4842,4835,4830,4832,4832,4815,4821,4816,4737,5143,4847,4823,4812,4802,4794,4789,4792,4790,4787,4769,4782,4780,4697};
const u8 g_MapVaMutual_Y[][2] =
{
    {0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,9},
    {0,10},{0,11},{0,12},{0,13},{255,255},{1,0},{1,1},{1,2},{1,3},{1,4},
    {1,5},{1,6},{1,7},{1,8},{1,9},{1,10},{1,11},{1,12},{1,13},{255,255},
    {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},{2,8},{2,9},
    {2,10},{2,11},{2,12},{2,13},{255,255},{3,0},{3,1},{3,2},{3,3},{3,4},
    {3,5},{3,6},{3,7},{3,8},{3,9},{3,10},{3,11},{3,12},{3,13},{255,255},
    {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{4,7},{4,8},{4,9},
    {4,10},{4,11},{4,12},{4,13},{255,255},{5,0},{5,1},{5,2},{5,3},{5,4},
    {5,5},{5,6},{5,7},{5,8},{5,9},{5,10},{5,11},{5,12},{5,13},{255,255},
    {6,0},{6,1},{6,2},{6,3},{6,4},{6,5},{6,6},{6,7},{6,8},{6,9},
    {6,10},{6,11},{6,12},{6,13},{255,255},{7,0},{7,1},{7,2},{7,3},{7,4},
    {7,5},{7,6},{7,7},{7,8},{7,9},{7,10},{7,11},{7,12},{7,13},{255,255},
    {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7},{8,8},{8,9},
    {8,10},{8,11},{8,12},{8,13},{255,255},{9,0},{9,1},{9,2},{9,3},{9,4},
    {9,5},{9,6},{9,7},{9,8},{9,9},{9,10},{9,11},{9,12},{9,13},{255,255},
    {10,0},{10,1},{10,2},{10,3},{10,4},{10,5},{10,6},{10,7},{10,8},{10,9},
    {10,10},{10,11},{10,12},{10,13},{255,255},{11,0},{11,1},{11,2},{11,3},{11,4},
    {11,5},{11,6},{11,7},{11,8},{11,9},{11,10},{11,11},{11,12},{11,13},{255,255},
    {12,0},{12,1},{12,2},{12,3},{12,4},{12,5},{12,6},{12,7},{12,8},{12,9},
    {12,10},{12,11},{12,12},{12,13},{255,255},{13,0},{13,1},{13,2},{13,3},{13,4},
    {13,5},{13,6},{13,7},{13,8},{13,9},{13,10},{13,11},{13,12},{13,13},{255,255},
    {14,0},{14,1},{14,2},{14,3},{14,4},{14,5},{14,6},{14,7},{14,8},{14,9},
    {14,10},{14,11},{14,12},{14,13},{255,255},{15,0},{15,1},{15,2},{15,3},{15,4},
    {15,5},{15,6},{15,7},{15,8},{15,9},{15,10},{15,11},{15,12},{15,13},{255,255},
    {16,0},{16,1},{16,2},{16,3},{16,4},{16,5},{16,6},{16,7},{16,8},{16,9},
    {16,10},{16,11},{16,12},{16,13},{255,255},{17,0},{17,1},{17,2},{17,3},{17,4},
    {17,5},{17,6},{17,7},{17,8},{17,9},{17,10},{17,11},{17,12},{17,13},{255,255},
    {18,0},{18,1},{18,2},{18,3},{18,4},{18,5},{18,6},{18,7},{18,8},{18,9},
    {18,10},{18,11},{18,12},{18,13},{255,255},{19,0},{19,1},{19,2},{19,3},{19,4},
    {19,5},{19,6},{19,7},{19,8},{19,9},{19,10},{19,11},{19,12},{19,13},{255,255},
    {20,0},{20,1},{20,2},{20,3},{20,4},{20,5},{20,6},{20,7},{20,8},{20,9},
    {20,10},{20,11},{20,12},{20,13},{255,255},{21,0},{21,1},{21,2},{21,3},{21,4},
    {21,5},{21,6},{21,7},{21,8},{21,9},{21,10},{21,11},{21,12},{21,13},{255,255},
    {22,0},{22,1},{22,2},{22,3},{22,4},{22,5},{22,6},{22,7},{22,8},{22,9},
    {22,10},{22,11},{22,12},{22,13},{255,255},{23,0},{23,1},{23,2},{23,3},{23,4},
    {23,5},{23,6},{23,7},{23,8},{23,9},{23,10},{23,11},{23,12},{23,13},{255,255},
};
