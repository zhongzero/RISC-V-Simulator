#include<bits/stdc++.h>
#define MaxIns 32
#define MaxRS 32
#define MaxROB 32
#define MaxSLB 32
#define MaxReg 32
#define IndexSize 5
#define MaxICache 32
using namespace std;

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
	unsigned int pos;
	while(~scanf("%s",tmp)){
		if(tmp[0]=='@')pos=Get0x(tmp,1,strlen(tmp)-1);
		else {
			mem[pos]=Get0x(tmp,0,strlen(tmp)-1);
			// printf("! %x %x\n",pos,mem[pos]);
			pos++;
		}
	}
}

class Order{
public:
	Ordertype type=END;
	unsigned int rd,rs1,rs2,imm;
	Order(){}
	Order(Ordertype _type,unsigned int _rd,unsigned int _rs1,unsigned int _rs2, unsigned int _imm):type(_type),rd(_rd),rs1(_rs1),rs2(_rs2),imm(_imm){}
};

int Clock=0;
int OKnum=0;
int predictTot=0,predictSuccess=0;
unsigned int pc_new;
unsigned int pc_las;
class Register{
public:
	unsigned int reg,reorder;
	bool busy;
}reg_las[MaxReg],reg_new[MaxReg];
class Ins_node{
public:
	unsigned int inst,pc,jumppc;
	bool isjump;
	Ordertype ordertype;
};
class Insturction_Queue{
public:
	Ins_node s[MaxIns];
	int L=1,R=0,size=0;
	bool is_waiting_ins=0;
}Ins_queue_las,Ins_queue_new;
class RS_node{
public:
	Ordertype ordertype;
	unsigned int inst,pc,jumppc,vj,vk,qj,qk,A,reorder;
	bool busy;
};
class ReservationStation{
public:
	RS_node s[MaxRS];
}RS_las,RS_new;

class SLB_node{
public:
	Ordertype ordertype;
	unsigned int inst,pc,vj,vk,qj,qk,A,reorder;
	bool ready;
};
class SLBuffer{
public:
	SLB_node s[MaxSLB];
	int L=1,R=0,size=0;
	bool is_waiting_data;
}SLB_las,SLB_new;

class ROB_node{
public:
	Ordertype ordertype;
	unsigned int inst,pc,jumppc,dest,value;
	bool isjump;
	bool ready;
};
class ReorderBuffer{
public:
	ROB_node s[MaxROB];
	int L=1,R=0,size=0;
}ROB_las,ROB_new;

class Branch_History_Table{
public:
	bool s[2];// 00 强不跳； 01 弱不跳； 10 弱跳； 11 弱不跳；
}BHT_las[1<<12],BHT_new[1<<12];
int update_BHT_id;

class MemCtrl{//一次只能曾内存中读取1byte(8bit)
public:
	unsigned int ins_addr;
	int ins_remain_cycle=0;
	int ins_current_pos=0;
	unsigned int ins_ans;// to InstructionQueue
	bool ins_ok=0;


	bool data_l_or_s;//0:load,1:store
	unsigned int data_addr;
	int data_remain_cycle=0;
	int data_current_pos=0;
	unsigned int data_in;//for_store
	unsigned int data_ans;//for_load
	bool data_ok;

}memctrl_las,memctrl_new;

//ram mem(交互用)
bool las_need_return=0;
unsigned int lasans;

// tag    index  offset
// [31:5] [4:0]  empty
struct ICache{//direct mapping
	bool valid[MaxICache];
	int tag[MaxICache];
	unsigned int inst[MaxICache];
}icache_las,icache_new;
void Search_In_ICache(unsigned int addr,bool &hit,unsigned int &returnInst){
	int b=addr&(MaxICache-1);
	if(icache_las.valid[b]&&icache_las.tag[b]==(addr>>IndexSize)){
		hit=1;
		returnInst=icache_las.inst[b];
	}
	else hit=0;
}
void Store_In_ICache(unsigned int addr,unsigned int storeInst){
	int b=addr&(MaxICache-1);
	icache_new.valid[b]=1;
	icache_new.tag[b]=(addr>>IndexSize);
	icache_new.inst[b]=storeInst;
}

Order Decode(unsigned int s,bool IsOutput=0){
	// cout<<"@@"<<s<<endl;
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
	if(order.type==JALR||order.type==LB||order.type==LH||order.type==LW||order.type==LBU||order.type==LHU){
		if(order.imm>>11)order.imm|=0xfffff000;
	}
	if(order.type==ADDI||order.type==SLTI||order.type==SLTIU||order.type==XORI||order.type==ORI||order.type==ANDI){
		if(order.imm>>11)order.imm|=0xfffff000;
	}
	if(order.type==SB||order.type==SH||order.type==SW){
		if(order.imm>>11)order.imm|=0xfffff000;
	}
	if(order.type==JAL){
		if(order.imm>>20)order.imm|=0xfff00000;
	}
	if(order.type==BEQ||order.type==BNE||order.type==BLT||order.type==BGE||order.type==BLTU||order.type==BGEU){
		if(order.imm>>12)order.imm|=0xffffe000;
	}

	// if(IsOutput){
	// 	if(s!=0x513)return order;
	// 	printf("%x\n",s);
	// 	cout<<order.type<<endl;
	// 	cout<<GGG[order.type]<<endl;
	// 	printf("%d %d %d\n",order.rd,order.rs1,order.imm);
	// }
	
	return order;
}
bool isCalc(Ordertype type){
	return type==LUI||type==AUIPC||type==ADD||type==SUB||type==SLL||type==SLT||type==SLTU||
			type==XOR||type==SRL||type==SRA||type==OR||type==AND||type==ADDI||type==SLTI||type==SLTIU||
			type==XORI||type==ORI||type==ANDI||type==SLLI||type==SRLI||type==SRAI;
}
bool isBranch(Ordertype type){
	return type==BEQ||type==BNE||type==BLT||type==BGE||type==BLTU||type==BGEU||type==JAL||type==JALR;
}
bool isLoad(Ordertype type){
	return type==LB||type==LH||type==LW||type==LBU||type==LHU;
}
bool isStore(Ordertype type){
	return type==SB||type==SH||type==SW;
}

bool flag_END_las,flag_END_new;
bool BranchJudge(int x){
	if(BHT_las[x].s[0]==0)return 0;
	return 1;
}
void Get_ins_to_queue(){
	bool hit;
	unsigned int inst;
	if(!Ins_queue_las.is_waiting_ins&&Ins_queue_las.size!=MaxIns){
		Search_In_ICache(pc_las,hit,inst);
		if(!hit){
			memctrl_new.ins_addr=pc_las;
			memctrl_new.ins_remain_cycle=4;
			Ins_queue_new.is_waiting_ins=1;
		}
	}
	if(memctrl_las.ins_ok){
		Ins_queue_new.is_waiting_ins=0;
		// cout<<"!!!"<<pc_las<<endl;
		// unsigned int inst=(unsigned int)mem[pc_las]+((unsigned int)mem[pc_las+1]<<8)+((unsigned int)mem[pc_las+2]<<16)+((unsigned int)mem[pc_las+3]<<24);
		inst=memctrl_las.ins_ans;
		Store_In_ICache(pc_las,memctrl_las.ins_ans);
	}
	if(memctrl_las.ins_ok||hit){
		// cout<<"!!!"<<pc_las<<endl;
		Order order=Decode(inst,1);
		if(order.type==END){flag_END_new=1;return;}
		Ins_node tmp;

		tmp.inst=inst,tmp.ordertype=order.type,tmp.pc=pc_las;
		if(isBranch(order.type)){
			//JAL 直接跳转
			//目前强制pc不跳转；JALR默认不跳转，让它必定预测失败
			if(order.type==JAL)pc_new=pc_las+order.imm;
			else {
				if(order.type==JALR)pc_new=pc_las+4;
				else {
					tmp.jumppc=pc_las+order.imm;
					if(BranchJudge(tmp.inst&0xfff))pc_new=pc_las+order.imm,tmp.isjump=1;
					else pc_new=pc_las+4,tmp.isjump=0;
				}
			}
		}
		else pc_new=pc_las+4;
		int g=(Ins_queue_las.R+1)%MaxIns;
		Ins_queue_new.R=g;
		Ins_queue_new.s[g]=tmp;
		Ins_queue_new.size++;
	}
}
void do_ins_queue(){
	//InstructionQueue为空，因此取消issue InstructionQueue中的指令
	if(Ins_queue_las.size==0)return;
	//ROB满了，因此取消issue InstructionQueue中的指令
	if(ROB_las.size==MaxROB)return;
	Ins_node tmp=Ins_queue_las.s[Ins_queue_las.L];

	if(isLoad(tmp.ordertype)||isStore(tmp.ordertype)){ //load指令(LB,LH,LW,LBU,LHU) or store指令(SB,SH,SW)
		
		//SLB满了，因此取消issue InstructionQueue中的指令
		if(SLB_las.size==MaxSLB)return;
		//b为该指令SLB准备存放的位置
		int r=(SLB_las.R+1)%MaxSLB;
		SLB_new.R=r,SLB_new.size++;
		//b为该指令ROB准备存放的位置
		int b=(ROB_las.R+1)%MaxROB;
		ROB_new.R=b,ROB_new.size++;
		//将该指令从Ins_queue删去
		Ins_queue_new.L=(Ins_queue_las.L+1)%MaxIns;
		Ins_queue_new.size--;
		//解码
		Order order=Decode(tmp.inst);
		
		//修改ROB
		
		ROB_new.s[b].pc=tmp.pc;
		ROB_new.s[b].inst=tmp.inst, ROB_new.s[b].ordertype=tmp.ordertype;
		ROB_new.s[b].dest=order.rd , ROB_new.s[b].ready=0;
		
		//修改LB

		//根据rs1寄存器的情况决定是否给其renaming(vj,qj)
		//如果rs1寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
		if(reg_las[order.rs1].busy){
			unsigned int h=reg_las[order.rs1].reorder;
			if(ROB_new.s[h].ready)SLB_new.s[r].vj=ROB_new.s[h].value,SLB_new.s[r].qj=-1;
			else SLB_new.s[r].qj=h;
		}
		else SLB_new.s[r].vj=reg_las[order.rs1].reg,SLB_new.s[r].qj=-1;

		if(isStore(tmp.ordertype)){// store类型  （有rs2的）
			//根据rs2寄存器的情况决定是否给其renaming(vk,qk)
			//如果rs2寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order.rs2].busy){
				unsigned int h=reg_las[order.rs2].reorder;
				if(ROB_new.s[h].ready)SLB_new.s[r].vk=ROB_new.s[h].value,SLB_new.s[r].qk=-1;
				else SLB_new.s[r].qk=h;
			}
			else SLB_new.s[r].vk=reg_las[order.rs2].reg,SLB_new.s[r].qk=-1;
		}
		else SLB_new.s[r].qk=-1;
		
		SLB_new.s[r].inst=tmp.inst , SLB_new.s[r].ordertype=tmp.ordertype;
		SLB_new.s[r].pc=tmp.pc;
		SLB_new.s[r].A=order.imm , SLB_new.s[r].reorder=b;
		if(isStore(tmp.ordertype))SLB_new.s[r].ready=0;

		//修改register

		if(!isStore(tmp.ordertype)){//不为 store指令  (其他都有rd)
			reg_new[order.rd].reorder=b,reg_new[order.rd].busy=1;
		}
	}
	else {// 计算(LUI,AUIPC,ADD,SUB...) or 无条件跳转(BEQ,BNE,BLE...) or 有条件跳转(JAL,JALR)
		
		//找到一个空的RS的位置，r为找到的空的RS的位置
		int r=-1;
		for(int i=0;i<MaxRS;i++)if(!RS_las.s[i].busy){r=i;break;}
		//RS满了，因此取消issue InstructionQueue中的指令
		if(r==-1)return;
		//b为该指令ROB准备存放的位置
		int b=(ROB_las.R+1)%MaxROB;
		ROB_new.R=b,ROB_new.size++;
		//将该指令从Ins_queue删去
		Ins_queue_new.L=(Ins_queue_las.L+1)%MaxIns;
		Ins_queue_new.size--;
		//解码
		Order order=Decode(tmp.inst);

		//修改ROB
		
		ROB_new.s[b].inst=tmp.inst, ROB_new.s[b].ordertype=tmp.ordertype;
		ROB_new.s[b].pc=tmp.pc, ROB_new.s[b].jumppc=tmp.jumppc , ROB_new.s[b].isjump=tmp.isjump;
		ROB_new.s[b].dest=order.rd , ROB_new.s[b].ready=0;

		//修改RS
		if( (tmp.inst&0x7f)!=0x37&&(tmp.inst&0x7f)!=0x17 && (tmp.inst&0x7f)!=0x6f ){// 不为LUI,AUIPC,JAL (有rs1的)
			//根据rs1寄存器的情况决定是否给其renaming(vj,qj)
			//如果rs1寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order.rs1].busy){
				unsigned int h=reg_las[order.rs1].reorder;
				if(ROB_new.s[h].ready){
					RS_new.s[r].vj=ROB_new.s[h].value,RS_new.s[r].qj=-1;
				}
				else RS_new.s[r].qj=h;
			}
			else RS_new.s[r].vj=reg_las[order.rs1].reg,RS_new.s[r].qj=-1;
		}
		else RS_new.s[r].qj=-1;

		if( (tmp.inst&0x7f)==0x33 || (tmp.inst&0x7f)==0x63){// (ADD..AND) or 有条件跳转  （有rs2的）
			//根据rs2寄存器的情况决定是否给其renaming(vk,qk)
			//如果rs2寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order.rs2].busy){
				unsigned int h=reg_las[order.rs2].reorder;
				if(ROB_new.s[h].ready)RS_new.s[r].vk=ROB_new.s[h].value,RS_new.s[r].qk=-1;
				else RS_new.s[r].qk=h;
			}
			else RS_new.s[r].vk=reg_las[order.rs2].reg,RS_new.s[r].qk=-1;
		}
		else RS_new.s[r].qk=-1;
		

		RS_new.s[r].inst=tmp.inst , RS_new.s[r].ordertype=tmp.ordertype;
		RS_new.s[r].pc=tmp.pc , RS_new.s[r].jumppc=tmp.jumppc;
		RS_new.s[r].A=order.imm , RS_new.s[r].reorder=b;
		RS_new.s[r].busy=1;

		//修改register
		if((tmp.inst&0x7f)!=0x63){//不为 有条件跳转  (其他都有rd)
			reg_new[order.rd].reorder=b,reg_new[order.rd].busy=1;
		}
	}
}
void EX(RS_node tmp,unsigned int &value,unsigned int &jumppc){
	if(tmp.ordertype==LUI)value=tmp.A;
	if(tmp.ordertype==AUIPC)value=tmp.pc+tmp.A;

	if(tmp.ordertype==ADD)value=tmp.vj+tmp.vk;
	if(tmp.ordertype==SUB)value=tmp.vj-tmp.vk;
	if(tmp.ordertype==SLL)value=tmp.vj<<(tmp.vk&0x1f);
	if(tmp.ordertype==SLT)value=((int)tmp.vj<(int)tmp.vk)?1:0;
	if(tmp.ordertype==SLTU)value=(tmp.vj<tmp.vk)?1:0;
	if(tmp.ordertype==XOR)value=tmp.vj^tmp.vk;
	if(tmp.ordertype==SRL)value=tmp.vj>>(tmp.vk&0x1f);
	if(tmp.ordertype==SRA)value=(int)tmp.vj>>(tmp.vk&0x1f);
	if(tmp.ordertype==OR)value=tmp.vj|tmp.vk;
	if(tmp.ordertype==AND)value=tmp.vj&tmp.vk;

	if(tmp.ordertype==JALR){
		jumppc=(tmp.vj+tmp.A)&(~1);
		value=tmp.pc+4;
	}


	if(tmp.ordertype==ADDI)value=tmp.vj+tmp.A;
	if(tmp.ordertype==SLTI)value=((int)tmp.vj<(int)tmp.A)?1:0;
	if(tmp.ordertype==SLTIU)value=(tmp.vj<tmp.A)?1:0;
	if(tmp.ordertype==XORI)value=tmp.vj^tmp.A;
	if(tmp.ordertype==ORI)value=tmp.vj|tmp.A;
	if(tmp.ordertype==ANDI)value=tmp.vj&tmp.A;
	if(tmp.ordertype==SLLI)value=tmp.vj<<tmp.A;
	if(tmp.ordertype==SRLI)value=tmp.vj>>tmp.A;
	if(tmp.ordertype==SRAI)value=(int)tmp.vj>>tmp.A;
	

	if(tmp.ordertype==JAL){
		// printf("imm=%x\n",tmp.A);
		value=tmp.pc+4;
	}


	if(tmp.ordertype==BEQ){
		value=(tmp.vj==tmp.vk?1:0);
	}
	if(tmp.ordertype==BNE){
		value=(tmp.vj!=tmp.vk?1:0);
	}
	if(tmp.ordertype==BLT){
		value=((int)tmp.vj<(int)tmp.vk?1:0);
	}
	if(tmp.ordertype==BGE){
		value=((int)tmp.vj>=(int)tmp.vk?1:0);
	}
	if(tmp.ordertype==BLTU){
		value=(tmp.vj<tmp.vk?1:0);
	}
	if(tmp.ordertype==BGEU){
		value=(tmp.vj>=tmp.vk?1:0);
	}
}
void do_RS(){
	for(int i=0;i<MaxRS;i++){
		if(RS_las.s[i].busy&&RS_las.s[i].qj==-1&&RS_las.s[i].qk==-1){
			unsigned value,jumppc;
			EX(RS_las.s[i],value,jumppc);

			// 修改 ROB
			int b=RS_las.s[i].reorder;
			ROB_new.s[b].value=value , ROB_new.s[b].ready=1;
			if(RS_las.s[i].ordertype==JALR)ROB_new.s[b].jumppc=jumppc;

			// 修改 RS
			RS_new.s[i].busy=0;
			for(int j=0;j<MaxRS;j++){
				if(RS_las.s[j].busy){
					if(RS_las.s[j].qj==b)RS_new.s[j].qj=-1,RS_new.s[j].vj=value;
					if(RS_las.s[j].qk==b)RS_new.s[j].qk=-1,RS_new.s[j].vk=value;
				}
			}

			// 修改 SLB
			for(int j=0;j<MaxSLB;j++){
				if(SLB_las.s[j].qj==b)SLB_new.s[j].qj=-1,SLB_new.s[j].vj=value;
				if(SLB_las.s[j].qk==b)SLB_new.s[j].qk=-1,SLB_new.s[j].vk=value;
			}
			break;
		}
	}
}
void LoadData(SLB_node tmp){
	memctrl_new.data_l_or_s=0;
	if(tmp.ordertype==LB){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=1;
	}
	if(tmp.ordertype==LH){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=2;
	}
	if(tmp.ordertype==LW){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=4;
	}
	if(tmp.ordertype==LBU){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=1;
	}
	if(tmp.ordertype==LHU){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=2;
	}
}
void Extend_LoadData(SLB_node tmp,unsigned int data,unsigned int &ans){
	//signed:符号位扩展，unsigned：0扩展
	if(tmp.ordertype==LB){
		if((ans>>7)&1)ans=data|(0xffffff00);
		else ans=data&(0x000000ff);
	}
	if(tmp.ordertype==LH){
		if((ans>>15)&1)ans=data|(0xffff0000);
		else ans=data&(0x0000ffff);
	}
	if(tmp.ordertype==LW)ans=data;
	if(tmp.ordertype==LBU)ans=data&(0x000000ff);
	if(tmp.ordertype==LHU)ans=data&(0x0000ffff);
}
void StoreData(SLB_node tmp){
	memctrl_new.data_l_or_s=1;
	memctrl_new.data_in=tmp.vk;
	if(tmp.ordertype==SB){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=1;
	}
	if(tmp.ordertype==SH){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=2;
	}
	if(tmp.ordertype==SW){
		unsigned int pos=tmp.vj+tmp.A;
		memctrl_new.data_addr=pos;
		memctrl_new.data_remain_cycle=4;
	}
}
void do_SLB(){
	if(memctrl_las.data_ok){
		SLB_new.is_waiting_data=0;
		int r=SLB_las.L;
		if(isLoad(SLB_las.s[r].ordertype)){
			unsigned int loadvalue;
			Extend_LoadData(SLB_las.s[r],memctrl_las.data_ans,loadvalue);
			// 更改 ROB
			int b=SLB_las.s[r].reorder;
			ROB_new.s[b].value=loadvalue , ROB_new.s[b].ready=1;
			// cout<<"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ loadvalue="<<loadvalue<<endl;

			// 更改 RS
			for(int j=0;j<MaxRS;j++){
				if(RS_las.s[j].busy){
					if(RS_las.s[j].qj==b)RS_new.s[j].qj=-1,RS_new.s[j].vj=loadvalue;
					if(RS_las.s[j].qk==b)RS_new.s[j].qk=-1,RS_new.s[j].vk=loadvalue;
				}
			}

			// 更改 SLB
			SLB_new.size--,SLB_new.L=(SLB_las.L+1)%MaxSLB;
			for(int j=0;j<MaxSLB;j++){
				if(SLB_las.s[j].qj==b)SLB_new.s[j].qj=-1,SLB_new.s[j].vj=loadvalue;
				if(SLB_las.s[j].qk==b){
					SLB_new.s[j].qk=-1,SLB_new.s[j].vk=loadvalue;
				}
			}

		}
		else {
			// 更改 ROB
			int b=SLB_las.s[r].reorder;
			ROB_new.s[b].ready=1;

			// 更改 SLB
			SLB_new.size--,SLB_new.L=(SLB_las.L+1)%MaxSLB;
		}
	}

	if(!SLB_las.is_waiting_data&&SLB_las.size){
		int r=SLB_las.L;
		if(isLoad(SLB_las.s[r].ordertype)){
			if(SLB_las.s[r].qj==-1){
				LoadData(SLB_las.s[r]);
				SLB_new.is_waiting_data=1;
			}
		}
		else {
			memctrl_new.data_l_or_s=1;
			if(SLB_las.s[r].qj==-1&&SLB_las.s[r].qk==-1&&SLB_las.s[r].ready){
				StoreData(SLB_las.s[r]);
				SLB_new.is_waiting_data=1;
			}
		}
	}
}
void ClearAll(){
	// clear ins_queue
	Ins_queue_new.L=1,Ins_queue_new.R=0,Ins_queue_new.size=0,Ins_queue_new.is_waiting_ins=0;
	
	// clear RS
	for(int i=0;i<MaxRS;i++)RS_new.s[i].busy=0;

	// clear SLB
	SLB_new.L=1,SLB_new.R=0,SLB_new.size=0,SLB_new.is_waiting_data=0;

	// clear ROB
	ROB_new.L=1,ROB_new.R=0,ROB_new.size=0;
	for(int i=0;i<MaxROB;i++)ROB_new.s[i].ready=0;

	//clear reg
	for(int i=0;i<MaxReg;i++)reg_new[i].busy=0;

	//clear memctrl
	memctrl_new.ins_remain_cycle=0,memctrl_new.ins_current_pos=0,memctrl_new.ins_ok=0;
	memctrl_new.data_remain_cycle=0,memctrl_new.data_current_pos=0,memctrl_new.data_ok=0;

	//icache
	for(int i=0;i<MaxICache;i++)icache_new.valid[i]=0;

	//ram
	las_need_return=0;

	// clear flag_END
	flag_END_new=0;

}
bool Clear_flag=0;
void do_ROB(){
	if(!ROB_las.size)return;
	int b=ROB_las.L;

	if(isBranch(ROB_las.s[b].ordertype)){
		if(!ROB_las.s[b].ready)return;
		OKnum++;
		
		// update ROB
		ROB_new.size--,ROB_new.L=(ROB_las.L+1)%MaxROB;

		//JAL必定预测成功
		//让JALR必定预测失败
		if(ROB_las.s[b].ordertype==JAL){

			// update register
			int rd=ROB_las.s[b].dest;
			reg_new[rd].reg=ROB_las.s[b].value;
			if(reg_las[rd].busy&&reg_las[rd].reorder==b)reg_new[rd].busy=0;
			// 更改 RS
			for(int j=0;j<MaxRS;j++){
				if(RS_las.s[j].busy){
					if(RS_las.s[j].qj==b)RS_new.s[j].qj=-1,RS_new.s[j].vj=ROB_las.s[b].value;
					if(RS_las.s[j].qk==b)RS_new.s[j].qk=-1,RS_new.s[j].vk=ROB_las.s[b].value;
				}
			}
			// 更改 SLB
			for(int j=0;j<MaxSLB;j++){
				if(SLB_las.s[j].qj==b)SLB_new.s[j].qj=-1,SLB_new.s[j].vj=ROB_las.s[b].value;
				if(SLB_las.s[j].qk==b)SLB_new.s[j].qk=-1,SLB_new.s[j].vk=ROB_las.s[b].value;
			}
		}
		else {
			if(ROB_las.s[b].ordertype!=JALR)predictTot++;

			if( (ROB_las.s[b].value^ROB_las.s[b].isjump)==1 || ROB_las.s[b].ordertype==JALR){//预测失败

				// update BHT
				int x=ROB_las.s[b].inst&0xfff;
				if(BHT_las[x].s[0]==0&&BHT_las[x].s[1]==0)BHT_new[x].s[0]=0,BHT_new[x].s[1]=1;
				if(BHT_las[x].s[0]==0&&BHT_las[x].s[1]==1)BHT_new[x].s[0]=1,BHT_new[x].s[1]=0;
				if(BHT_las[x].s[0]==1&&BHT_las[x].s[1]==0)BHT_new[x].s[0]=0,BHT_new[x].s[1]=1;
				if(BHT_las[x].s[0]==1&&BHT_las[x].s[1]==1)BHT_new[x].s[0]=1,BHT_new[x].s[1]=0;
				update_BHT_id=x;

				if(ROB_las.s[b].value)pc_new=ROB_las.s[b].jumppc;
				else pc_new=ROB_las.s[b].pc+4;
				Clear_flag=1;
				if(ROB_las.s[b].ordertype==JALR){
					
					// update register
					int rd=ROB_las.s[b].dest;
					reg_new[rd].reg=ROB_las.s[b].value;
					if(reg_las[rd].busy&&reg_las[rd].reorder==b)reg_new[rd].busy=0;

					// 更改 RS
					for(int j=0;j<MaxRS;j++){
						if(RS_las.s[j].busy){
							if(RS_las.s[j].qj==b)RS_new.s[j].qj=-1,RS_new.s[j].vj=ROB_las.s[b].value;
							if(RS_las.s[j].qk==b)RS_new.s[j].qk=-1,RS_new.s[j].vk=ROB_las.s[b].value;
						}
					}
					// 更改 SLB
					for(int j=0;j<MaxSLB;j++){
						if(SLB_las.s[j].qj==b)SLB_new.s[j].qj=-1,SLB_new.s[j].vj=ROB_las.s[b].value;
						if(SLB_las.s[j].qk==b)SLB_new.s[j].qk=-1,SLB_new.s[j].vk=ROB_las.s[b].value;
					}
				}
				return;
			}
			else {//预测成功
				predictSuccess++;
				// update BHT
				int x=ROB_las.s[b].inst&0xfff;
				if(BHT_las[x].s[0]==0&&BHT_las[x].s[1]==0)BHT_new[x].s[0]=0,BHT_new[x].s[1]=0;
				if(BHT_las[x].s[0]==0&&BHT_las[x].s[1]==1)BHT_new[x].s[0]=0,BHT_new[x].s[1]=0;
				if(BHT_las[x].s[0]==1&&BHT_las[x].s[1]==0)BHT_new[x].s[0]=1,BHT_new[x].s[1]=1;
				if(BHT_las[x].s[0]==1&&BHT_las[x].s[1]==1)BHT_new[x].s[0]=1,BHT_new[x].s[1]=1;
				update_BHT_id=x;
				return;
			}
		}
	}
	else if(isStore(ROB_las.s[b].ordertype)){
		if(!ROB_las.s[b].ready){
			// update SLB
			for(int i=0;i<MaxSLB;i++){
				if(SLB_las.s[i].reorder==b){
					SLB_new.s[i].ready=1;
				}
			}
			return;
		}
		OKnum++;
		// update ROB
		ROB_new.size--,ROB_new.L=(ROB_las.L+1)%MaxROB;
		return;
	}
	else {//Load or calc
		if(!ROB_las.s[b].ready)return;
		OKnum++;
		
		// update ROB
		ROB_new.size--,ROB_new.L=(ROB_las.L+1)%MaxROB;

		// update register
		int rd=ROB_las.s[b].dest;
		reg_new[rd].reg=ROB_las.s[b].value;
		if(reg_las[rd].busy&&reg_las[rd].reorder==b)reg_new[rd].busy=0;

		// 更改 RS
		for(int j=0;j<MaxRS;j++){
			if(RS_las.s[j].busy){
				if(RS_las.s[j].qj==b)RS_new.s[j].qj=-1,RS_new.s[j].vj=ROB_las.s[b].value;
				if(RS_las.s[j].qk==b)RS_new.s[j].qk=-1,RS_new.s[j].vk=ROB_las.s[b].value;
			}
		}

		// 更改 SLB
		for(int j=0;j<MaxSLB;j++){
			if(SLB_las.s[j].qj==b)SLB_new.s[j].qj=-1,SLB_new.s[j].vj=ROB_las.s[b].value;
			if(SLB_las.s[j].qk==b)SLB_new.s[j].qk=-1,SLB_new.s[j].vk=ROB_las.s[b].value;
		}

		return;
	}
}
void update(){
	pc_las=pc_new;
	for(int i=0;i<MaxReg;i++)reg_las[i]=reg_new[i];
	reg_las[0].reg=0,reg_las[0].busy=0;//0号寄存器强制为0
	Ins_queue_las=Ins_queue_new;
	RS_las=RS_new;
	SLB_las=SLB_new;
	ROB_las=ROB_new;
	BHT_las[update_BHT_id]=BHT_new[update_BHT_id];
	memctrl_las=memctrl_new;
	icache_las=icache_new;
	flag_END_las=flag_END_new;
	Clear_flag=0;
}
void mem_ram(bool en_in,bool r_or_w,unsigned int addr,unsigned char data_in,unsigned char &data_ans){//与内存交互 (0:r,1:w)
	if(las_need_return){
		data_ans=lasans;
	}
	las_need_return=0;
	if(en_in){
		if(r_or_w==0){//read
			las_need_return=1;
			lasans=mem[addr];
		}
		else{//write
			mem[addr]=data_in;
		}
	}
}
void do_memctrl(){
	if(!(memctrl_las.data_remain_cycle&&memctrl_las.data_l_or_s==0&&memctrl_las.data_remain_cycle==5)&&
		!(memctrl_las.data_remain_cycle&&memctrl_las.data_l_or_s==1&&memctrl_las.data_remain_cycle==1) ){
		memctrl_new.data_ok=0;
	}
	if( ! (!memctrl_las.data_remain_cycle&&memctrl_las.ins_remain_cycle) ){
		memctrl_new.ins_ok=0;
	}
	if(memctrl_las.data_remain_cycle){
		if(memctrl_las.data_l_or_s==0){//load
			// unsigned unsigned int pos=memctrl_las.data_addr;
			// if(memctrl_las.data_remain_cycle==4){
			// 	memctrl_new.data_ans=(unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8)+((unsigned int)mem[pos+2]<<16)+((unsigned int)mem[pos+3]<<24);
			// }
			// if(memctrl_las.data_remain_cycle==2){
			// 	memctrl_new.data_ans=(unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8);
			// }
			// if(memctrl_las.data_remain_cycle==1){
			// 	memctrl_new.data_ans=(unsigned int)mem[pos];
			// }
			// memctrl_new.data_ok=1;
			// memctrl_new.data_remain_cycle=0;

			
			if(memctrl_las.data_remain_cycle==4){
				memctrl_new.data_remain_cycle=3;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==3){
				memctrl_new.data_remain_cycle=2;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==2){
				memctrl_new.data_remain_cycle=1;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==1){
				memctrl_new.data_remain_cycle=5;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==5){
				memctrl_new.data_remain_cycle=0;
				memctrl_new.data_current_pos=0;
				memctrl_new.data_ok=1;
			}

			unsigned char data_in,data_ans;// data_in : meaningless
			bool needvalue=(1<=memctrl_las.data_remain_cycle&&memctrl_las.data_remain_cycle<=4);
			mem_ram(needvalue,0,memctrl_las.data_addr,data_in,data_ans);

			if(memctrl_las.data_current_pos==1){
				memctrl_new.data_ans=(memctrl_las.data_ans&0xffffff00)|data_ans;//[7:0]
			}
			if(memctrl_las.data_current_pos==2){
				memctrl_new.data_ans=(memctrl_las.data_ans&0xffff00ff)|((unsigned int)data_ans<<8);//[15:8]
			}
			if(memctrl_las.data_current_pos==3){
				memctrl_new.data_ans=(memctrl_las.data_ans&0xff00ffff)|((unsigned int)data_ans<<16);//[23:16]
			}
			if(memctrl_las.data_current_pos==4){
				memctrl_new.data_ans=(memctrl_las.data_ans&0x00ffffff)|((unsigned int)data_ans<<24);//[31:24]
			}

			// if(memctrl_new.data_ok==1){
			// 	cout<<memctrl_new.data_ans<<endl;
			// }
		}
		else {//store
			// unsigned int pos=memctrl_las.data_addr;
			// if(memctrl_las.data_remain_cycle==4){
			// 	mem[pos]=memctrl_las.data_in&0xff,mem[pos+1]=(memctrl_las.data_in>>8)&0xff,mem[pos+2]=(memctrl_las.data_in>>16)&0xff,mem[pos+3]=(memctrl_las.data_in>>24)&0xff;
			// 	cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las.data_in<<endl;
			// 	cout<<pos+1<<" "<<(unsigned int)mem[pos+1]<<" "<<memctrl_las.data_in<<endl;
			// 	cout<<pos+2<<" "<<(unsigned int)mem[pos+2]<<" "<<memctrl_las.data_in<<endl;
			// 	cout<<pos+3<<" "<<(unsigned int)mem[pos+3]<<" "<<memctrl_las.data_in<<endl;
			// }
			// if(memctrl_las.data_remain_cycle==2){
			// 	mem[pos]=memctrl_las.data_in&0xff,mem[pos+1]=(memctrl_las.data_in>>8)&0xff;
			// 	cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las.data_in<<endl;
			// 	cout<<pos+1<<" "<<(unsigned int)mem[pos+1]<<" "<<memctrl_las.data_in<<endl;
			// }
			// if(memctrl_las.data_remain_cycle==1){
			// 	mem[pos]=memctrl_las.data_in&0xff;
			// 	cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las.data_in<<endl;
			// }
			// memctrl_new.data_ok=1;
			// memctrl_new.data_remain_cycle=0;


			unsigned char data_in,data_ans;// data_ans : meaningless

			if(memctrl_las.data_remain_cycle==4){
				memctrl_new.data_remain_cycle=3;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==3){
				memctrl_new.data_remain_cycle=2;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==2){
				memctrl_new.data_remain_cycle=1;
				memctrl_new.data_current_pos=memctrl_las.data_current_pos+1;
				memctrl_new.data_addr=memctrl_las.data_addr+1;
			}
			if(memctrl_las.data_remain_cycle==1){
				memctrl_new.data_remain_cycle=0;
				memctrl_new.data_current_pos=0;
				memctrl_new.data_ok=1;
			}

			if(memctrl_las.data_current_pos==0){
				data_in=(memctrl_las.data_in&0x000000ff);//[7:0]
			}
			if(memctrl_las.data_current_pos==1){
				data_in=(memctrl_las.data_in&0x0000ff00)>>8;//[15:8]
			}
			if(memctrl_las.data_current_pos==2){
				data_in=(memctrl_las.data_in&0x00ff0000)>>16;//[23:16]
			}
			if(memctrl_las.data_current_pos==3){
				data_in=(memctrl_las.data_in&0xff000000)>>24;//[31:24]
			}
			// cout<<memctrl_las.data_addr<<" "<<(unsigned int)data_in<<" "<<memctrl_las.data_in<<endl;
			

			bool needvalue=(1<=memctrl_las.data_remain_cycle&&memctrl_las.data_remain_cycle<=4);
			mem_ram(needvalue,1,memctrl_las.data_addr,data_in,data_ans);
		}
	}
	else if(memctrl_las.ins_remain_cycle){
		unsigned int tmppc=memctrl_las.ins_addr;
		//memctrl_las.data_remain_cycle==4
		memctrl_new.ins_ans=(unsigned int)mem[tmppc]+((unsigned int)mem[tmppc+1]<<8)+((unsigned int)mem[tmppc+2]<<16)+((unsigned int)mem[tmppc+3]<<24);
		memctrl_new.ins_ok=1;
		memctrl_new.ins_remain_cycle=0;
	}
}
int main(){
	GetData();
	pc_new=pc_las=0;
	flag_END_new=flag_END_las=0;
	while(1){
		Clock++;
		// cout<<"clock="<<Clock<<endl;
		do_memctrl();
		Get_ins_to_queue();
		do_ROB();
		do_ins_queue();
		//Get_ins_to_queue()要在do_ROB()前面，因为同时修改了pc_new，但do_ROB()优先级更高(do_ROB()中的remake)
		//do_ROB()要在do_ins_queue()前面，因为同时修改了reg_new，但do_ins_queue()优先级更高
		do_RS();
		do_SLB();
		if(Clear_flag)ClearAll();
		update();
		// if(OKnum==7000){
		// 	cout<<"Ins_queue.size="<<Ins_queue_las.size<<endl;
		// 	cout<<"SLB.size="<<SLB_las.size<<endl;
		// 	cout<<"ROB.size="<<ROB_las.size<<endl;
		// 	cout<<"ROB.L="<<ROB_las.L<<endl;
		// 	cout<<"ROB.L type="<<GGG[ROB_las.s[ROB_las.L].ordertype]<<endl;
		// 	cout<<OKnum<<endl;
		// 	cout<<Clock<<endl;
		// 	for(int i=0;i<MaxReg;i++)cout<<reg_las[i].reg<<" ";
		// 	cout<<endl;
		// }
		if(flag_END_las&&Ins_queue_las.size==0&&ROB_las.size==0)break;
		// if(Clock==20040)exit(0);
	}
	printf("%u\n",reg_las[10].reg&255u);
	// printf("Clock=%d\n",Clock);
	// printf("predict success: %d/%d=%lf\n",predictSuccess,predictTot,predictSuccess*1.0/predictTot);
	return 0;
}