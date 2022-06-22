#include<bits/stdc++.h>
using namespace std;
unsigned int reg[32],pc;
unsigned char mem[500000];
unsigned int Get0x(char *s,int l,int r){
	unsigned int ans=0;
	for(int i=l;i<=r;i++){
		if('0'<=s[i]&&s[i]<='9')ans=(ans<<4)|(s[i]-'0');
		else ans=(ans<<4)|(s[i]-'A'+10);
	}
	return ans;
}
void GetData(){
	char tmp[20];
	int pos;
	while(~scanf("%s",tmp)){
		if(tmp[0]=='@')pos=Get0x(tmp,1,strlen(tmp)-1);
		else {
			mem[pos]=Get0x(tmp,0,strlen(tmp)-1);
			// printf("! %x %x\n",pos,mem[pos]);
			pos++;
		}
	}
}
enum Ordertype{
    LUI,AUIPC,  //U类型 用于操作长立即数的指令 0~1
	ADD,SUB,SLL,SLT,SLTU,XOR,SRL,SRA,OR,AND,  //R类型 寄存器间操作指令 2~11
    JALR,LB,LH,LW,LBU,LHU,ADDI,SLTI,SLTIU,XORI,ORI,ANDI,SLLI,SRLI,SRAI,  //I类型 短立即数和访存Load操作指令 12~26
    SB,SH,SW,  //S类型 访存Store操作指令 27~29
    JAL,  //J类型 用于无条件跳转的指令 30
    BEQ,BNE,BLT,BGE,BLTU,BGEU,  //B类型 用于有条件跳转的指令 31~36
	END
};
string GGG[]={"LUI","AUIPC",
	"ADD","SUB","SLL","SLT","SLTU","XOR","SRL","SRA","OR","AND",
    "JALR","LB","LH","LW","LBU","LHU","ADDI","SLTI","SLTIU","XORI","ORI","ANDI","SLLI","SRLI","SRAI",
    "SB","SH","SW",
    "JAL",
    "BEQ","BNE","BLT","BGE","BLTU","BGEU",
	"END"};
class Order{
public:
	Ordertype type;
	unsigned int rd,rs1,rs2,imm;
	Order(){}
	Order(Ordertype _type,unsigned int _rd,unsigned int _rs1,unsigned int _rs2, unsigned int _imm):type(_type),rd(_rd),rs1(_rs1),rs2(_rs2),imm(_imm){}
};
Order AnalysisOrder(int pos){
	unsigned int s=(unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8)+((unsigned int)mem[pos+2]<<16)+((unsigned int)mem[pos+3]<<24);
	Order order;
	if(s==(int)0x0ff00513){
		order.type=END;
		return order;
	}
	int type1=s&0x7f;
	int type2=(s>>12)&0x7;
	int type3=(s>>25)&0x7f;
	order.rd=(s>>7)&0x1f;
	order.rs1=(s>>15)&0x1f;
	order.rs2=(s>>20)&0x1f;
	// printf("%x %x %x %x %x %x\n",type3,order.rs2,order.rs1,type2,order.rd,type1);
	if(type1==0x37||type1==0x17){//U类型
		if(type1==0x37)order.type=LUI;
		if(type1==0x17)order.type=AUIPC;
		order.imm=(s>>12)<<12;
	}
	if(type1==0x33){//R类型
		if(type2==0x0){
			if(type3==0x00)order.type=ADD;
			if(type3==0x20)order.type=SUB;
		}
		if(type2==0x1)order.type=SLL;
		if(type2==0x2)order.type=SLT;
		if(type2==0x3)order.type=SLTU;
		if(type2==0x4)order.type=XOR;
		if(type2==0x5){
			if(type3==0x00)order.type=SRL;
			if(type3==0x20)order.type=SRA;
		}
		if(type2==0x6)order.type=OR;
		if(type2==0x7)order.type=AND;
	}
	if(type1==0x67||type1==0x03||type1==0x13){//I类型
		if(type1==0x67)order.type=JALR;
		if(type1==0x03){
			if(type2==0x0)order.type=LB;
			if(type2==0x1)order.type=LH;
			if(type2==0x2)order.type=LW;
			if(type2==0x4)order.type=LBU;
			if(type2==0x5)order.type=LHU;
		}
		if(type1==0x13){
			if(type2==0x0)order.type=ADDI;
			if(type2==0x2)order.type=SLTI;
			if(type2==0x3)order.type=SLTIU;
			if(type2==0x4)order.type=XORI;
			if(type2==0x6)order.type=ORI;
			if(type2==0x7)order.type=ANDI;
			if(type2==0x1)order.type=SLLI;
			if(type2==0x5){
				if(type3==0x00)order.type=SRLI;
				if(type3==0x20)order.type=SRAI;
			}
		}
		order.imm=(s>>20);
	}
	if(type1==0x23){//S类型
		if(type2==0x0)order.type=SB;
		if(type2==0x1)order.type=SH;
		if(type2==0x2)order.type=SW;
		order.imm=((s>>25)<<5) | ((s>>7)&0x1f);
	}
	if(type1==0x6f){//J类型
		order.type=JAL;
		order.imm=(((s>>12)&0xff)<<12) | (((s>>20)&0x1)<<11) | (((s>>21)&0x3ff)<<1)  | (((s>>31)&1)<<20);
	}
	if(type1==0x63){//B类型
		if(type2==0x0)order.type=BEQ;
		if(type2==0x1)order.type=BNE;
		if(type2==0x4)order.type=BLT;
		if(type2==0x5)order.type=BGE;
		if(type2==0x6)order.type=BLTU;
		if(type2==0x7)order.type=BGEU;
		order.imm=(((s>>7)&0x1)<<11) | (((s>>8)&0xf)<<1) | (((s>>25)&0x3f)<<5)  | (((s>>31)&1)<<12);
	}
	// printf("%x %u\n",s,reg[10]&255u);
	// printf("%d %x\n",pc,s);
	// cout<<GGG[order.type]<<endl;
	
	return order;
}
void work(Order order){
	// printf("%x %d\n",pc,order.type);
	if(order.type==LUI)reg[order.rd]=order.imm,pc+=4;
	if(order.type==AUIPC)reg[order.rd]=pc+order.imm,pc+=4;

	if(order.type==ADD)reg[order.rd]=reg[order.rs1]+reg[order.rs2],pc+=4;
	if(order.type==SUB)reg[order.rd]=reg[order.rs1]-reg[order.rs2],pc+=4;
	if(order.type==SLL)reg[order.rd]=reg[order.rs1]<<(reg[order.rs2]&0x1f),pc+=4;
	if(order.type==SLT)reg[order.rd]=((int)reg[order.rs1]<(int)reg[order.rs2])?1:0,pc+=4;
	if(order.type==SLTU)reg[order.rd]=(reg[order.rs1]<reg[order.rs2])?1:0,pc+=4;
	if(order.type==XOR)reg[order.rd]=reg[order.rs1]^reg[order.rs2],pc+=4;
	if(order.type==SRL)reg[order.rd]=reg[order.rs1]>>(reg[order.rs2]&0x1f),pc+=4;
	if(order.type==SRA)reg[order.rd]=(int)reg[order.rs1]>>(reg[order.rs2]&0x1f),pc+=4;
	if(order.type==OR)reg[order.rd]=reg[order.rs1]|reg[order.rs2],pc+=4;
	if(order.type==AND)reg[order.rd]=reg[order.rs1]&reg[order.rs2],pc+=4;

	if(order.type==JALR||order.type==LB||order.type==LH||order.type==LW||order.type==LBU||order.type==LHU){
		if(order.imm>>11)order.imm|=0xfffff000;
	}
	if(order.type==JALR){
		unsigned int t=pc+4;
		pc=(reg[order.rs1]+order.imm)&(~1);
		reg[order.rd]=t;
	}
	if(order.type==LB){
		int pos=reg[order.rs1]+order.imm;
		reg[order.rd]=(char)mem[pos];
		pc+=4;
	}
	if(order.type==LH){
		int pos=reg[order.rs1]+order.imm;
		reg[order.rd]=(short)((unsigned short)mem[pos]+((unsigned short)mem[pos+1]<<8));
		pc+=4;
	}
	if(order.type==LW){
		int pos=reg[order.rs1]+order.imm;
		reg[order.rd]=(int)((unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8)+((unsigned int)mem[pos+2]<<16)+((unsigned int)mem[pos+3]<<24));
		pc+=4;
	}
	if(order.type==LBU){
		int pos=reg[order.rs1]+order.imm;
		reg[order.rd]=mem[pos];
		pc+=4;
	}
	if(order.type==LHU){
		int pos=reg[order.rs1]+order.imm;
		reg[order.rd]=(unsigned short)mem[pos]+((unsigned short)mem[pos+1]<<8);
		pc+=4;
	}

	if(order.type==ADDI||order.type==SLTI||order.type==SLTIU||order.type==XORI||order.type==ORI||order.type==ANDI){
		if(order.imm>>11)order.imm|=0xfffff000;
	}
	if(order.type==ADDI)reg[order.rd]=reg[order.rs1]+order.imm,pc+=4;
	if(order.type==SLTI)reg[order.rd]=((int)reg[order.rs1]<(int)order.imm)?1:0,pc+=4;
	if(order.type==SLTIU)reg[order.rd]=(reg[order.rs1]<order.imm)?1:0,pc+=4;
	if(order.type==XORI)reg[order.rd]=reg[order.rs1]^order.imm,pc+=4;
	if(order.type==ORI)reg[order.rd]=reg[order.rs1]|order.imm,pc+=4;
	if(order.type==ANDI)reg[order.rd]=reg[order.rs1]&order.imm,pc+=4;
	if(order.type==SLLI)reg[order.rd]=reg[order.rs1]<<order.imm,pc+=4;
	if(order.type==SRLI)reg[order.rd]=reg[order.rs1]>>order.imm,pc+=4;
	if(order.type==SRAI)reg[order.rd]=(int)reg[order.rs1]>>order.imm,pc+=4;
	
	if(order.type==SB||order.type==SH||order.type==SW){
		if(order.imm>>11)order.imm|=0xfffff000;
	}

	if(order.type==SB){
		int pos=reg[order.rs1]+order.imm;
		mem[pos]=reg[order.rs2]&0xff;
		pc+=4;
	}
	if(order.type==SH){
		int pos=reg[order.rs1]+order.imm;
		mem[pos]=reg[order.rs2]&0xff,mem[pos+1]=(reg[order.rs2]>>8)&0xff;
		pc+=4;
	}
	if(order.type==SW){
		int pos=reg[order.rs1]+order.imm;
		mem[pos]=reg[order.rs2]&0xff,mem[pos+1]=(reg[order.rs2]>>8)&0xff,mem[pos+2]=(reg[order.rs2]>>16)&0xff,mem[pos+3]=(reg[order.rs2]>>24)&0xff;
		pc+=4;
	}

	if(order.type==JAL){
		if(order.imm>>20)order.imm|=0xfff00000;
	}

	if(order.type==JAL){
		// printf("imm=%x\n",order.imm);
		reg[order.rd]=pc+4,pc+=order.imm;
	}

	if(order.type==BEQ||order.type==BNE||order.type==BLT||order.type==BGE||order.type==BLTU||order.type==BGEU){
		if(order.imm>>12)order.imm|=0xffffe000;
	}

	if(order.type==BEQ){
		if(reg[order.rs1]==reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	if(order.type==BNE){
		if(reg[order.rs1]!=reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	if(order.type==BLT){
		if((int)reg[order.rs1]<(int)reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	if(order.type==BGE){
		if((int)reg[order.rs1]>=(int)reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	if(order.type==BLTU){
		if(reg[order.rs1]<reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	if(order.type==BGEU){
		if(reg[order.rs1]>=reg[order.rs2])pc+=order.imm;
		else pc+=4;
	}
	reg[0]=0;
	// printf("%d %d %d %d !!! ",order.rd,order.rs1,order.rs2,order.imm);
	// for(int i=0;i<=20;i++)cout<<reg[i]<<" ";
	// cout<<endl;
}

int main(){
	int cnt=0;
	GetData();
	pc=0;
	while(1){
		Order order=AnalysisOrder(pc);
		if(order.type==END)break;
		work(order);
		// if(++cnt==500)exit(0);
	}
	printf("%u\n",reg[10]&255u);
	return 0;
}