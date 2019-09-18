#include <reg52.h>
#include <intrins.h>

#define uchar unsigned char		
#define uint  unsigned int		

sfr ISP_DATA  = 0xe2;			// 数据寄存器
sfr ISP_ADDRH = 0xe3;			// 地址寄存器高八位
sfr ISP_ADDRL = 0xe4;			// 地址寄存器低八位
sfr ISP_CMD   = 0xe5;			// 命令寄存器
sfr ISP_TRIG  = 0xe6;			// 命令触发寄存器
sfr ISP_CONTR = 0xe7;			// 命令寄存器

sbit LcdRs_P  = P0^7;     // 1602液晶的RS管脚       
sbit LcdRw_P  = P0^6;     // 1602液晶的RW管脚 
sbit LcdEn_P  = P0^5;     // 1602液晶的EN管脚
sbit Subkey   = P3^4;			// 减键
sbit Addkey   = P3^5;			// 加键
sbit Buzzer_P = P3^6;			// 蜂鸣器
sbit Led_P    = P1^0;			// LED报警灯
sbit Left_P   = P3^0;			// 左边的红外传感器
sbit Right_P  = P3^1;			// 右边的红外传感器

uint Alarm;         // 饱和警戒数量
uint input=0;	    // 物料进入数量									
uint out=0;	    // 物料出去数量
uint Now=0;	    // 当前物料存留数量


/*********************************************************/
// 单片机内部EEPROM不使能
/*********************************************************/
void ISP_Disable()
{
	ISP_CONTR = 0;
	ISP_ADDRH = 0;
	ISP_ADDRL = 0;
}


/*********************************************************/
// 从单片机内部EEPROM读一个字节，从0x2000地址开始
/*********************************************************/
unsigned char EEPROM_Read(unsigned int add)
{
	ISP_DATA  = 0x00;
	ISP_CONTR = 0x83;
	ISP_CMD   = 0x01;
	ISP_ADDRH = (unsigned char)(add>>8);
	ISP_ADDRL = (unsigned char)(add&0xff);
	// 对STC89C51系列来说，每次要写入0x46，再写入0xB9,ISP/IAP才会生效
	ISP_TRIG  = 0x46;	   
	ISP_TRIG  = 0xB9;
	_nop_();
	ISP_Disable();
	return (ISP_DATA);
}


/*********************************************************/
// 往单片机内部EEPROM写一个字节，从0x2000地址开始
/*********************************************************/
void EEPROM_Write(unsigned int add,unsigned char ch)
{
	ISP_CONTR = 0x83;
	ISP_CMD   = 0x02;
	ISP_ADDRH = (unsigned char)(add>>8);
	ISP_ADDRL = (unsigned char)(add&0xff);
	ISP_DATA  = ch;
	ISP_TRIG  = 0x46;
	ISP_TRIG  = 0xB9;
	_nop_();
	ISP_Disable();
}


/*********************************************************/
// 擦除单片机内部EEPROM的一个扇区
// 写8个扇区中随便一个的地址，便擦除该扇区，写入前要先擦除
/*********************************************************/
void Sector_Erase(unsigned int add)	  
{
	ISP_CONTR = 0x83;
	ISP_CMD   = 0x03;
	ISP_ADDRH = (unsigned char)(add>>8);
	ISP_ADDRL = (unsigned char)(add&0xff);
	ISP_TRIG  = 0x46;
	ISP_TRIG  = 0xB9;
	_nop_();
	ISP_Disable();
}


/*********************************************************/
// 毫秒级的延时函数，time是要延时的毫秒数
/*********************************************************/
void DelayMs(uint time)
{
	uint i,j;
	for(i=0;i<time;i++)
		for(j=0;j<112;j++);
}


/*********************************************************/
// 1602液晶写命令函数，cmd就是要写入的命令
/*********************************************************/
void LcdWriteCmd(uchar cmd)
{ 
	LcdRs_P = 0;
	LcdRw_P = 0;
	LcdEn_P = 0;
	P2=cmd;
	DelayMs(2);
	LcdEn_P = 1;    
	DelayMs(2);
	LcdEn_P = 0;	
}


/*********************************************************/
// 1602液晶写数据函数，dat就是要写入的数据
/*********************************************************/
void LcdWriteData(uchar dat)
{
	LcdRs_P = 1; 
	LcdRw_P = 0;
	LcdEn_P = 0;
	P2=dat;
	DelayMs(2);
	LcdEn_P = 1;    
	DelayMs(2);
	LcdEn_P = 0;
}


/*********************************************************/
// 1602液晶初始化函数
/*********************************************************/
void LcdInit()
{
	LcdWriteCmd(0x38);        // 16*2显示，5*7点阵，8位数据口
	LcdWriteCmd(0x0C);        // 开显示，不显示光标
	LcdWriteCmd(0x06);        // 地址加1，当写入数据后光标右移
	LcdWriteCmd(0x01);        // 清屏
}


/*********************************************************/
// 液晶光标定位函数
/*********************************************************/
void LcdGotoXY(uchar line,uchar column)
{
	// 第一行
	if(line==0)        
		LcdWriteCmd(0x80+column); 
	 // 第二行
	if(line==1)        
		LcdWriteCmd(0x80+0x40+column); 
}


/*********************************************************/
// 液晶输出字符串函数
/*********************************************************/
void LcdPrintStr(uchar *str)
{
	while(*str!='\0')
		LcdWriteData(*str++);
}


/*********************************************************/
// 液晶输出数字
/*********************************************************/
void LcdPrintNum(uint num)
{
	LcdWriteData(num/1000+48);			// 千位	
	LcdWriteData(num%1000/100+48);	// 百位
	LcdWriteData(num%100/10+48);		// 十位
	LcdWriteData(num%10+48); 				// 个位
}


/*********************************************************/
// 液晶显示初始化
/*********************************************************/
void LcdShowInit()
{
	LcdGotoXY(0,0);											// 液晶光标定位到第0行
	LcdPrintStr("IN:0000 OUT:0000");		// 液晶第0行显示"IN:0000  OUT:0000"
	LcdGotoXY(1,0);											// 液晶光标定位到第1行
	LcdPrintStr("RM:0000 ALM:    ");		// 液晶第1行显示"RM:0000  ALM:    "
}



/*********************************************************/
// 按键扫描
/*********************************************************/
void KeyScanf()
{
	if(Subkey==0)												// 如果减按键被按下	
	{
		if(Alarm>1)											// 只有Alarm大于1才能减1								
			Alarm--;				
		LcdGotoXY(1,12);									// 液晶光标定位到第1行第11列
		LcdPrintNum(Alarm);							// 刷新改变后的报警值
		DelayMs(250);											// 延时一下
		Sector_Erase(0x2000);							// 存入EEPROM前先擦除
		EEPROM_Write(0x2000,Alarm/100);	// 报警值存入EEPROM
		EEPROM_Write(0x2001,Alarm%100);
	}
	
	if(Addkey==0)												// 如果加按键被按下	
	{
		if(Alarm<9999)										// 只有Alarm小于9999才能加1
			Alarm++;				
		LcdGotoXY(1,12);									// 液晶光标定位到第1行第11列
		LcdPrintNum(Alarm);							// 刷新改变后的报警值
		DelayMs(250);											// 延时一下
		Sector_Erase(0x2000);							// 存入EEPROM前先擦除
		EEPROM_Write(0x2000,Alarm/100);	// 报警值存入EEPROM
		EEPROM_Write(0x2001,Alarm%100);
	}
}


/*********************************************************/
// 报警判断
/*********************************************************/
void AlarmJudge()
{
	if(Now>=Alarm)
	{
		Buzzer_P=0;		// 蜂鸣器报警
		Led_P=0;			// LED灯亮
	}
	else
	{
		Buzzer_P=1;		// 蜂鸣器停止报警
		Led_P=1;			// LED熄灭
	}	
}


/*********************************************************/
// 主函数
/*********************************************************/
void main(void)
{
	LcdInit();				    			// 液晶功能初始化	   
	LcdShowInit();							// 液晶显示内容初始化  两步初始化
	
	Alarm=EEPROM_Read(0x2000)*100+EEPROM_Read(0x2001);			// 从EEPROM中读取报警值
	if((Alarm==0)||(Alarm>9999))										// 如果读出来数据异常，则重新赋值20
		Alarm=20;
	
	LcdGotoXY(1,12);						// 液晶光标定位到第1行第11列
	LcdPrintNum(Alarm);				// 显示报警值
	
	while(1)
	{
		if(Left_P==0)							// 如果左边的红外模块检测到物料进入
		{
			if(input<9999)						// 判断当前物料进入数量是否小于9999
			{
				input++;								// 进入数量加1
				LcdGotoXY(0,3);				// 光标定位
				LcdPrintNum(input);		// 显示物料进入数量
				LcdGotoXY(1,3);				// 光标定位
				Now=input-out;				// 计算当前物料存留数量
				LcdPrintNum(Now);		// 显示当前物料存留数量
			}
			Buzzer_P=0;							// 蜂鸣器嘀一声
			DelayMs(30);
			Buzzer_P=1;
			while(!Left_P);					// 等待物料离开左边的传感器检测范围
			DelayMs(100);
		}
		
		if(Right_P==0)						// 如果右边的红外模块检测到有物料出去
		{
			if(out<input)						// 判断当前物料出去数量是否小于物料进入数量
			{
				out++;								// 出去数量加1
				LcdGotoXY(0,12);			// 光标定位
				LcdPrintNum(out);		// 显示物料出去数量
				LcdGotoXY(1,3);				// 光标定位
				Now=input-out;				// 计算当前物料存留数量
				LcdPrintNum(Now);		// 显示当前物料存留数量
			}
			Buzzer_P=0;							// 蜂鸣器嘀一声
			DelayMs(30);
			Buzzer_P=1;
			while(!Right_P);				// 等待物料离开右边的传感器检测范围
			DelayMs(100);
		}

		AlarmJudge();							// 判断是否需要报警
		
		KeyScanf();								// 按键扫描			
	}
}

