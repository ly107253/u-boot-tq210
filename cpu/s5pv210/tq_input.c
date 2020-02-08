#include <common.h>
 
#ifdef CONFIG_EMBEDSKY_INPUT 
#include <regs.h>
#include <tq_input.h>
#include <lcd/s5pv210_fb.h>
#ifdef KEY_DEBUG
#define dbg(x...)       printf(x)
#else
#define dbg(x...)       do { } while (0)
#endif
#if(USER_ADC_12BIT_MODE == 1)
const Point Touch_Base_dot[4]={{123,800},{123,73},{981,800},{981,73}};
#else
const Point Touch_Base_dot[4]={{798,4095},{798,689},{3656,4095},{3656,689}};
#endif
const Point Lcd_Base_dot[4]={{0,0},{480,0},{0,272},{480,272}};

#if (LCD_TFT == DYNAMIC_CONFIG)
const lcd_config_info tq_lcd_config_info[LCD_TYPE_MAX] = {
//240x320
{
		1,//VBPD
		2,//VFPD
		1,//VSPW
		6,//HBPD
		3,//HFPD
		1,//HSPW
		2,//CLKVAL
		24,//BPP
		240,//XSIZE
		320,//YSIZE
},
//320x240
{

		12,//VBPD
		4,//VFPD
		5,//VSPW
		21,//HBPD
		32,//HFPD
		44,//HSPW
		6,//CLKVAL
		24,//BPP
		240,//XSIZE
		320,//YSIZE
		
},
//480x272
{
		2,//VBPD
		2,//VFPD
		10,//VSPW
		2,//HBPD
		2,//HFPD
		40,//HSPW
		7,//CLKVAL
		24,//BPP
		480,//XSIZE
		272,//YSIZE
},
//640x480
{
		29,//VBPD
		5,//VFPD
		4,//VSPW
		71,//HBPD
		40,//HFPD
		31,//HSPW
		1,//CLKVAL
		24,//BPP
		640,//XSIZE
		480,//YSIZE

},
//800x480
{
		9,//10,//4,//VBPD
		21,//22,//2,//VFPD
		12,//13,//2,//VSPW
		26,//27,//HBPD
		13,//14,//HFPD
		19,//20,//10,//HSPW
		4,//6,//CLKVAL
		24,//BPP
		800,//XSIZE
		480,//YSIZE

},
//800x600
{
		10,//VBPD
		22,//VFPD
		13,//VSPW
		10,//HBPD
		10,//HFPD
		36,//HSPW
		4,//CLKVAL
		24,//BPP
		800,//XSIZE
		600,//YSIZE

},
//1024x768
{
		5,//VBPD
		3,//VFPD
		3,//VSPW
		27,//HBPD
		14,//HFPD
		10,//HSPW
		4,//CLKVAL
		24,//BPP
		1024,//1024,//XSIZE
		768,//768,//YSIZE

},
//VGA7213
{
		21,//29,//VBPD
		0,//15,//VFPD
		4,//2,//4,//VSPW
		88,//71,//HBPD
		38,//40,//HFPD
		128,//133,//31,//HSPW
		3,//6,//CLKVAL
		24,//BPP
		800,//600,//XSIZE
		600,//480,//YSIZE
},
//1280x720
{
		24,//VBPD
		1,//VFPD
		3,//VSPW
		200,//HBPD
		64,//HFPD
		136,//HSPW
		1,//4,//CLKVAL
		24,//BPP
		1280,//XSIZE
		720,//YSIZE

},
//1920x1080
{
		24,//VBPD
		1,//VFPD
		3,//VSPW
		200,//HBPD
		64,//HFPD
		136,//HSPW
		1,//4,//CLKVAL
		24,//BPP
		1920,//XSIZE
		1080,//YSIZE

},
//....other lcd
};
#endif /* (LCD_TFT == DYNAMIC_CONFIG) */

static void printf_adc_regs(void)
{
	s5pv210_tsadc *TSADC=s5pv210_GetBase_tsadc();
	printf("\nTSADC->TSADCCON0:\t0x%08x,\tadd:0x%x\n",TSADC->TSADCCON0,&TSADC->TSADCCON0);
	printf("TSADC->TSCON0:\t0x%08x,\tadd:0x%x\n",TSADC->TSCON0,&TSADC->TSCON0);
	printf("TSADC->TSDLY0:\t0x%08x,\tadd:0x%x\n",TSADC->TSDLY0,&TSADC->TSDLY0);
	printf("TSADC->TSDATX0:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATX0,&TSADC->TSDATX0);
	printf("TSADC->TSDATY0:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATY0,&TSADC->TSDATY0);
	printf("TSADC->TSPENSTAT0:\t0x%08x,\tadd:0x%x\n",TSADC->TSPENSTAT0,&TSADC->TSPENSTAT0);
	printf("TSADC->CLRINTADC0:\t0x%08x,\tadd:0x%x\n",TSADC->CLRINTADC0,&TSADC->CLRINTADC0);
	printf("TSADC->ADCMUX:\t0x%08x,\tadd:0x%x\n",TSADC->ADCMUX,&TSADC->ADCMUX);
	printf("TSADC->CLRINTPEN0:\t0x%08x,\tadd:0x%x\n",TSADC->CLRINTPEN0,&TSADC->CLRINTPEN0);
	printf("TSADC->TSADCCON1:\t0x%08x,\tadd:0x%x\n",TSADC->TSADCCON1,&TSADC->TSADCCON1);
	printf("TSADC->TSCON1:\t0x%08x,\tadd:0x%x\n",TSADC->TSCON1,&TSADC->TSCON1);
	printf("TSADC->TSDLY1:\t0x%08x,\tadd:0x%x\n",TSADC->TSDLY1,&TSADC->TSDLY1);
	printf("TSADC->TSDATX1:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATX1,&TSADC->TSDATX1);
	printf("TSADC->TSDATY1:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATY1,&TSADC->TSDATY1);
	printf("TSADC->TSPENSTAT1:\t0x%08x,\tadd:0x%x\n",TSADC->TSPENSTAT1,&TSADC->TSPENSTAT1);
	printf("TSADC->CLRINTADC1:\t0x%08x,\tadd:0x%x\n",TSADC->CLRINTADC1,&TSADC->CLRINTADC1);
	printf("TSADC->CLRINTPEN1:\t0x%08x,\tadd:0x%x\n",TSADC->CLRINTPEN1,&TSADC->CLRINTPEN1);
//	printf("TSADC->:0x%08x,\tadd:0x%x",TSADC->,&TSADC->);
	
}
static u32 Abs(int dat)
{
	if(dat>0)
	{
		return (u32) dat;
	}
	else    return (u32) (0-dat);
}
static Point get_screen_dot(Point touch_dot)
{
//LCD_XSIZE_TFT
//LCD_YSIZE_TFT
	//float ratio_x,ratio_y;
	u32 ratio_x,ratio_y;
	int lefttop_x,lefttop_y;
	Point s_dot={-1,-1};
/**********************************************
;	两点式校准:dot[3] dot[2]
;	x=x1*ratio_x + lefttop_x;
;	y=y1*ratio_y + lefttop_y;
***********************************************/
	printf("touch_dot(%d,%d);\n",touch_dot.x,touch_dot.y);	
	if((Touch_Base_dot[2].x-Touch_Base_dot[1].x) != 0)
	{
		ratio_x = (u32)Abs((Lcd_Base_dot[2].x-Lcd_Base_dot[1].x)*100)/
			   (u32)Abs((Touch_Base_dot[2].x-Touch_Base_dot[1].x));

		lefttop_x = Lcd_Base_dot[2].x - (ratio_x*Touch_Base_dot[2].x)/100;
		printf("ratio_x:%d,lefttop_x:%d\n",ratio_x,lefttop_x);
	}
	else
	{
		printf("dot error\n");
		return s_dot;
	}
	if((Touch_Base_dot[2].y-Touch_Base_dot[1].y) != 0)
	{
		ratio_y = (u32)Abs((Lcd_Base_dot[2].y-Lcd_Base_dot[1].y)*100)/
			   (u32)Abs((Touch_Base_dot[2].y-Touch_Base_dot[1].y));

		lefttop_y = Lcd_Base_dot[2].y - (ratio_y*Touch_Base_dot[2].y)/100;
		printf("ratio_y:%d,lefttop_y:%d\n",ratio_y,lefttop_y);
	}
	else
	{
		printf("dot error\n");
		return s_dot;
	}


	s_dot.x = (touch_dot.x*ratio_x)/100 + lefttop_x;
	s_dot.y = (touch_dot.y*ratio_y)/100 + lefttop_y;

	printf("s_dot(%d,%d);\n",s_dot.x,s_dot.y);	
	
	return s_dot;
}
void tq_touch_init(void)
{
	s5pv210_tsadc *TSADC=s5pv210_GetBase_tsadc();
	Point touch_dot;
	printf("rADC_CONTROL:0x%08x,\tadd:0x%x\n",rADC_CONTROL,&rADC_CONTROL);
	rADC_CONTROL |= 0x01;
	printf("rADC_CONTROL:0x%08x,\tadd:0x%x\n",rADC_CONTROL,&rADC_CONTROL);

	printf("rCLK_GATE_IP3:\t0x%08x,\tadd:0x%x\n",rCLK_GATE_IP3,&rCLK_GATE_IP3);
	rCLK_GATE_IP3 |= (1<<24);
	printf("rCLK_GATE_IP3:\t0x%08x,\tadd:0x%x\n",rCLK_GATE_IP3,&rCLK_GATE_IP3);

	printf("TQ210 touch initialzation.\n");
	printf_adc_regs();
	
	
	TSADC->TSDLY1	 = 0x5000;
	TSADC->TSADCCON0 |= (1<<17);
	TSADC->TSADCCON1 = (1<<17) | ADC_RES_MODE | (/*19*/65<<6) | (1<<14) /*| (1<<0)*/;
	TSADC->TSCON1 = 0xd3;//0x5c;
	

	while(1)
	{
#if 0
		if(TSADC->TSADCCON1 & (0x1<<15))
		{
			//TSADC->TSADCCON1 |= 0x1<<2;

			//printf("TSADC->TSDATX1:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATX1,&TSADC->TSDATX1);
			//printf("TSADC->TSDATY1:\t0x%08x,\tadd:0x%x\n",TSADC->TSDATY1,&TSADC->TSDATY1);

			if(TSADC->TSPENSTAT1 & 0x01)
			printf("down\n");
			if(TSADC->TSPENSTAT1 & (0x01<<1))
			printf("up\n");
			if(TSADC->TSDATX1 & (0x1<<15))
			printf("downx\n");
			if(TSADC->TSDATY1 & (0x1<<15))
			printf("downy\n");
		}
		//printf_adc_regs();
#else

		if(TSADC->TSPENSTAT1 & 0x01)
		{
			//printf("down\n");			//Touch screen state down
			if(TSADC->TSDATX1 & (1<<15))
			{
				TSADC->TSCON1 &= ~(1<<8);
			}
			TSADC->TSCON1 = (1<<3) | (1<<2);
			TSADC->TSDLY1	 = 0x5000;//50000;//0x5000;

			TSADC->TSADCCON1 |= (1<<0);		//start TSADC trun
			while(TSADC->TSADCCON1 & (0x01<<0));	//check bit 0

			while(!(TSADC->TSADCCON1 & (0x1<<15))); //check turn ok;
			
			printf("downdot(%d,%d);\n",(TSADC->TSDATX1 & 0xfff),(TSADC->TSDATY1 & 0xfff));

			touch_dot.x = (TSADC->TSDATX1 & 0xfff);
			touch_dot.y = (TSADC->TSDATY1 & 0xfff);
			get_screen_dot(touch_dot);
			TSADC->TSCON1 = 0xd3;
		        TSADC->TSCON1 |= (1<<8);	//0x5c;

			TSADC->TSDLY1	 = 0x5000;

		        TSADC->TSCON1 &= ~(1<<8);	

			TSADC->CLRINTADC1 |= 0x01;
			TSADC->CLRINTPEN1 |= 0x01;
			TSADC->TSPENSTAT1 &= ~0x01;
			//printf_adc_regs();

		}
		//printf_adc_regs();
#endif
	}

}

Point tq_read_touch(void)
{

}



unsigned char get_button_value(void)
{
	unsigned char key_o,key=0;
	
	key = rGPH0DAT&0x3f;
	key_o = key;
	if(key != 0x3f)
	{
	  	udelay(5000);
		key = rGPH0DAT&0x3f;
		if(key == key_o)
		{
			key = ~(key|0xc0);
			switch (key)
			{
			case (KEY0):
				dbg("KEY1 down!\n");
				break;
			case (KEY1):
				dbg("KEY2 down!\n");
				break;
			case (KEY2):
				dbg("KEY3 down!\n");
				break;
			case (KEY3):
				dbg("KEY4 down!\n");
				break;
			case (KEY4):
				dbg("KEY5 down!\n");
				break;
			case (KEY5):
				dbg("KEY6 down!\n");
				break;
			default:break;
			}
			while((key_o&0x3f) == (rGPH0DAT&0x3f));	//waiting for button release
			return key;
		}
		
	}
	return 0;
}
void tq_test_button(void)
{
	unsigned char key_val=0;
	tq_button_init();
	while(1)
	{
		key_val = get_button_value();
		switch (key_val)
		{
			case (KEY0):
				dbg("KEY1 up!\n");
				break;
			case (KEY1):
				dbg("KEY2 up!\n");
				break;
			case (KEY2):
				dbg("KEY3 up!\n");
				break;
			case (KEY3):
				dbg("KEY4 up!\n");
				break;
			case (KEY4):
				dbg("KEY5 up!\n");
				break;
			case (KEY5):
				dbg("KEY6 up!\n");
				break;
			default:break;
		}
	}	
}
unsigned char tq_input_sys(void)
{
	tq_button_init();
	return get_button_value();
}

#endif
