#include<bits/stdc++_h>
#define `MaxIns 32
#define `MaxRS 32
#define `MaxROB 32
#define `MaxSLB 32
#define `MaxReg 32
#define `IndexSize 5
#define `MaxICache 32
#define `MaxBHT (1<<12)
using namespace std;

enum Ordertype begin
    LUI;AUIPC;  //U类型 用于操作长立即数的指令 0~1
	ADD;SUB;SLL;SLT;SLTU;XOR;SRL;SRA;OR;AND;  //R类型 寄存器间操作指令 2~11
    JALR;LB;LH;LW;LBU;LHU;ADDI;SLTI;SLTIU;XORI;ORI;ANDI;SLLI;SRLI;SRAI;  //I类型 短立即数和访存Load操作指令 12~26
    SB;SH;SW;  //S类型 访存Store操作指令 27~29
    JAL;  //J类型 用于无条件跳转的指令 30
    BEQ;BNE;BLT;BGE;BLTU;BGEU;  //B类型 用于有条件跳转的指令 31~36
	END
end;
string GGG[]= begin"LUI";"AUIPC";
	"ADD";"SUB";"SLL";"SLT";"SLTU";"XOR";"SRL";"SRA";"OR";"AND";
    "JALR";"LB";"LH";"LW";"LBU";"LHU";"ADDI";"SLTI";"SLTIU";"XORI";"ORI";"ANDI";"SLLI";"SRLI";"SRAI";
    "SB";"SH";"SW";
    "JAL";
    "BEQ";"BNE";"BLT";"BGE";"BLTU";"BGEU";
	"END"end;

unsigned char mem[500000];
unsigned int Get0x(char *s;int l;int r) begin
	unsigned int ans=0;
	for(i=l;i<=r;i++) begin
		if('0'<=s&&[i]s<='[i]9')ans=(ans<<4)|(s-'[i]0');
		else ans=(ans<<4)|(s-'[i]A'+10);
	end
	return ans;
end
void GetData() begin
	char tmp[20];
	unsigned int pos;
	while(~scanf("%s";tmp)) begin
		if(tmp[0]=='@')pos=Get0x(tmp;1;strlen(tmp)-1);
		else  begin
			mem[pos]=Get0x(tmp;0;strlen(tmp)-1);
			// printf("! %x %x\n";pos;mem[pos]);
			pos++;
		end
	end
end

class Order begin
public:
	Ordertype type=END;
	unsigned int rd;rs1;rs2;imm;
	Order() beginend
	Order(Ordertype _type;unsigned int _rd;unsigned int _rs1;unsigned int _rs2; unsigned int _imm):type(_type);rd(_rd);rs1(_rs1);rs2(_rs2);imm(_imm) beginend
end;

int Clock=0;
int OKnum=0;
int predictTot=0;predictSuccess=0;
unsigned int pc;
unsigned int pc_las;
class Register begin
public:
	unsigned int reg;reorder;
	bool busy;
endreg_las[`MaxReg];reg[`MaxReg];
class Ins_node begin
public:
	unsigned inst;pc;jumppc;
	bool isjump;
	Ordertype ordertype;
end;
class Insturction_Queue begin
public:
	Ins_node s[`MaxIns];
	int L=1;R=0;size=0;
	bool is_waiting_ins=0;
endIns_queue_las;Ins_queue;
class RS_node begin
public:
	Ordertype ordertype;
	unsigned inst;pc;jumppc;vj;vk;qj;qk;A;reorder;
	bool busy;
end;
class ReservationStation begin
public:
	RS_node s[`MaxRS];
endRS_las;RS;

class SLB_node begin
public:
	Ordertype ordertype;
	unsigned inst;pc;vj;vk;qj;qk;A;reorder;
	bool ready;
end;
class SLBuffer begin
public:
	SLB_node s[`MaxSLB];
	int L=1;R=0`;size=0;
	bool is_waiting_data;
endSLB_las;SLB;

class ROB_node begin
public:
	Ordertype ordertype;
	unsigned inst;pc;jumppc;dest;value;
	bool isjump;
	bool ready;
end;
class ReorderBuffer begin
public:
	ROB_node s[`MaxROB];
	int L=1;R=0;size=0;
endROB_las;ROB;

class Branch_History_Table begin
public:
	bool s[2];// 00 强不跳； 01 弱不跳； 10 弱跳； 11 弱不跳；
endBHT_las[`MaxBHT];BHT[`MaxBHT];
int update_BHT_id;

class MemCtrl begin//一次只能从内存中读取1byte(8bit)
public:
	unsigned ins_addr;
	ins_remain_cycle=0;
	ins_current_pos=0;
	unsigned ins_ans;// to InstructionQueue
	bool ins_ok=0;


	bool data_l_or_s;//0:load;1:store
	unsigned int data_addr;
	int data_remain_cycle=0;
	int data_current_pos=0;
	unsigned int data_in;//for_store
	unsigned int data_ans;//for_load
	bool data_ok;

endmemctrl_las;memctrl;

//ram mem(交互用)
bool las_need_return=0;
unsigned int lasans;

// tag    index  offset
// [31:5] [4:0]  empty
struct ICache begin//direct mapping
	bool valid[`MaxICache];
	int tag[`MaxICache];
	unsigned inst[`MaxICache];
endicache_las;icache;

void init() begin
	//一部分初始值在定义里已经给出，一部分初始值在下面给出，剩余默认初始值为0
	
	// init RS
	for(i=0;i<`MaxRS;i++) begin
		RS_s_qj[i]=RS_s_qk[i]=-1;
		RS_las_s_qj[i]=RS_las_s_qk[i]=-1;
	end

	// init SLB
	for(i=0;i<`MaxSLB;i++) begin
		SLB_s[`i]_qj=SLB_s_qk[i]=-1;
		SLB_las_s_qj[i]=SLB_las_s_qk[i]=-1;
	end
end

void Search_In_ICache(unsigned int addr1;bool &hit;unsigned int &returnInst) begin
	int b1=addr1&(`MaxICache-1);
	if(icache_las_valid[b1]&&icache_las_tag[b1]==(addr1>>`IndexSize)) begin
		hit=1;
		returnInst=icache_las_inst[b1];
	end
	else hit=0;
end
void Store_In_ICache(unsigned int addr2;unsigned int storeInst) begin
	int b2=addr2&(`MaxICache-1);
	icache_valid[b2]=1;
	icache_tag[b2]=(addr2>>`IndexSize);
	icache_inst[b2]=storeInst;
end

Order Decode(unsigned int s;bool IsOutput=0) begin
	// cout<<"@@"<<s<<endl;
	Order order;
	if(s==(int)0x0ff00513) begin
		order_type=END;
		return order;
	end
	int type1=s&0x7f;
	int type2=(s>>12)&0x7;
	int type3=(s>>25)&0x7f;
	order_rd=(s>>7)&0x1f;
	order_rs1=(s>>15)&0x1f;
	order_rs2=(s>>20)&0x1f;
	if(type1==0x37||type1==0x17) begin//U类型
		if(type1==0x37)order_type=LUI;
		if(type1==0x17)order_type=AUIPC;
		order_imm=(s>>12)<<12;
	end
	if(type1==0x33) begin//R类型
		if(type2==0x0) begin
			if(type3==0x00)order_type=ADD;
			if(type3==0x20)order_type=SUB;
		end
		if(type2==0x1)order_type=SLL;
		if(type2==0x2)order_type=SLT;
		if(type2==0x3)order_type=SLTU;
		if(type2==0x4)order_type=XOR;
		if(type2==0x5) begin
			if(type3==0x00)order_type=SRL;
			if(type3==0x20)order_type=SRA;
		end
		if(type2==0x6)order_type=OR;
		if(type2==0x7)order_type=AND;
	end
	if(type1==0x67||type1==0x03||type1==0x13) begin//I类型
		if(type1==0x67)order_type=JALR;
		if(type1==0x03) begin
			if(type2==0x0)order_type=LB;
			if(type2==0x1)order_type=LH;
			if(type2==0x2)order_type=LW;
			if(type2==0x4)order_type=LBU;
			if(type2==0x5)order_type=LHU;
		end
		if(type1==0x13) begin
			if(type2==0x0)order_type=ADDI;
			if(type2==0x2)order_type=SLTI;
			if(type2==0x3)order_type=SLTIU;
			if(type2==0x4)order_type=XORI;
			if(type2==0x6)order_type=ORI;
			if(type2==0x7)order_type=ANDI;
			if(type2==0x1)order_type=SLLI;
			if(type2==0x5) begin
				if(type3==0x00)order_type=SRLI;
				if(type3==0x20)order_type=SRAI;
			end
		end
		order_imm=(s>>20);
	end
	if(type1==0x23) begin//S类型
		if(type2==0x0)order_type=SB;
		if(type2==0x1)order_type=SH;
		if(type2==0x2)order_type=SW;
		order_imm=((s>>25)<<5) | ((s>>7)&0x1f);
	end
	if(type1==0x6f) begin//J类型
		order_type=JAL;
		order_imm=(((s>>12)&0xff)<<12) | (((s>>20)&0x1)<<11) | (((s>>21)&0x3ff)<<1)  | (((s>>31)&1)<<20);
	end
	if(type1==0x63) begin//B类型
		if(type2==0x0)order_type=BEQ;
		if(type2==0x1)order_type=BNE;
		if(type2==0x4)order_type=BLT;
		if(type2==0x5)order_type=BGE;
		if(type2==0x6)order_type=BLTU;
		if(type2==0x7)order_type=BGEU;
		order_imm=(((s>>7)&0x1)<<11) | (((s>>8)&0xf)<<1) | (((s>>25)&0x3f)<<5)  | (((s>>31)&1)<<12);
	end
	if(order_type==JALR||order_type==LB||order_type==LH||order_type==LW||order_type==LBU||order_type==LHU) begin
		if(order_imm>>11)order_imm|=0xfffff000;
	end
	if(order_type==ADDI||order_type==SLTI||order_type==SLTIU||order_type==XORI||order_type==ORI||order_type==ANDI) begin
		if(order_imm>>11)order_imm|=0xfffff000;
	end
	if(order_type==SB||order_type==SH||order_type==SW) begin
		if(order_imm>>11)order_imm|=0xfffff000;
	end
	if(order_type==JAL) begin
		if(order_imm>>20)order_imm|=0xfff00000;
	end
	if(order_type==BEQ||order_type==BNE||order_type==BLT||order_type==BGE||order_type==BLTU||order_type==BGEU) begin
		if(order_imm>>12)order_imm|=0xffffe000;
	end

	// if(IsOutput) begin
	// 	if(s!=0x513)return order;
	// 	printf("%x\n";s);
	// 	cout<<order_type<<endl;
	// 	cout<<GGG[order_type]<<endl;
	// 	printf("%d %d %d\n";order_rd;order_rs1;order_imm);
	// end
	
	return order;
end
bool isCalc(Ordertype type) begin
	return type==LUI||type==AUIPC||type==ADD||type==SUB||type==SLL||type==SLT||type==SLTU||
			type==XOR||type==SRL||type==SRA||type==OR||type==AND||type==ADDI||type==SLTI||type==SLTIU||
			type==XORI||type==ORI||type==ANDI||type==SLLI||type==SRLI||type==SRAI;
end
bool isBranch(Ordertype type) begin
	return type==BEQ||type==BNE||type==BLT||type==BGE||type==BLTU||type==BGEU||type==JAL||type==JALR;
end
bool isLoad(Ordertype type) begin
	return type==LB||type==LH||type==LW||type==LBU||type==LHU;
end
bool isStore(Ordertype type) begin
	return type==SB||type==SH||type==SW;
end

bool flag_END_las;flag_END;
bool BranchJudge(int bht_id) begin
	if(BHT_las[bht_id]_s[0]==0)return 0;
	return 1;
end
void Get_ins_to_queue() begin
	bool hit=0;
	unsigned inst;
	if(!Ins_queue_las_is_waiting_ins&&Ins_queue_las_size!=`MaxIns) begin
		Search_In_ICache(pc_las;hit;inst);
		// hit=0;
		if(!hit) begin
			memctrl_ins_addr=pc_las;
			memctrl_ins_remain_cycle=4;
			Ins_queue_is_waiting_ins=1;
		end
	end
	if(memctrl_las_ins_ok) begin
		Ins_queue_is_waiting_ins=0;
		// cout<<"!!!"<<pc_las<<endl;
		// unsigned inst=(unsigned int)mem[pc_las]+((unsigned int)mem[pc_las+1]<<8)+((unsigned int)mem[pc_las+2]<<16)+((unsigned int)mem[pc_las+3]<<24);
		inst=memctrl_las_ins_ans;
		Store_In_ICache(pc_las;memctrl_las_ins_ans);
	end
	if(memctrl_las_ins_ok||hit) begin
		// cout<<memctrl_las_ins_ok<<" "<<hit<<endl;
		// cout<<"!!!"<<pc_las<<" "<<inst<<endl;
		Order order=Decode(inst;1);
		if(order_type==END) beginflag_END=1;return;end
		Ins_node tmp;

		tmp_inst=inst;tmp_ordertype=order_type;tmp_pc=pc_las;
		if(isBranch(order_type)) begin
			//JAL 直接跳转
			//目前强制pc不跳转；JALR默认不跳转，让它必定预测失败
			if(order_type==JAL)pc=pc_las+order_imm;
			else  begin
				if(order_type==JALR)pc=pc_las+4;
				else  begin
					tmp_jumppc=pc_las+order_imm;
					if(BranchJudge(tmp_inst&0xfff))pc=pc_las+order_imm;tmp_isjump=1;
					else pc=pc_las+4;tmp_isjump=0;
				end
			end
		end
		else pc=pc_las+4;
		int g=(Ins_queue_las_R+1)%`MaxIns;
		Ins_queue_R=g;
		Ins_queue_s[g]=tmp;
		Ins_queue_size++;
	end
end
void do_ins_queue() begin
	//InstructionQueue为空，因此取消issue InstructionQueue中的指令
	if(Ins_queue_las_size==0)return;
	//ROB满了，因此取消issue InstructionQueue中的指令
	if(ROB_las_size==`MaxROB)return;
	Ins_node tmp=Ins_queue_las_s[Ins_queue_las_L];

	if(isLoad(tmp_ordertype)||isStore(tmp_ordertype)) begin //load指令(LB;LH;LW;LBU;LHU) or store指令(SB;SH;SW)
		
		//SLB满了，因此取消issue InstructionQueue中的指令
		if(SLB_las_size==`MaxSLB)return;
		//b为该指令SLB准`备存放的位置
		int r=(SLB_las_R+1)%`MaxSLB;
		SLB_R=r;SLB_size++;
		//b为该指令ROB准备存放的位置
		int b=(ROB_las_R+1)%`MaxROB;
		ROB_R=b;ROB_size++;
		//将该指令从Ins_queue删去
		Ins_queue_L=(Ins_queue_las_L+1)%`MaxIns;
		Ins_queue_size--;
		//解码
		Order order=Decode(tmp_inst);
		
		//修改ROB
		
		ROB_s[b]_pc=tmp_pc;
		ROB_s[b]_inst=tmp_inst; ROB_s[b]_ordertype=tmp_ordertype;
		ROB_s[b]_dest=order_rd ; ROB_s[b]_ready=0;
		
		//修改SLB

		//根据rs1寄存器的情况决定是否给其renaming(vj;qj)
		//如果rs1寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
		if(reg_las[order_rs1]_busy) begin
			unsigned int h=reg_las[order_rs1]_reorder;
			if(ROB_s[h]_ready)SLB_s[r]_vj=ROB_s[h]_value;SLB_s[r]_qj=-1;
			else SLB_s[r]_qj=h;
		end
		else SLB_s[r]_vj=reg_las[order_rs1]_reg;SLB_s[r]_qj=-1;

		if(isStore(tmp_ordertype)) begin// store类型  （有rs2的）
			//根据rs2寄存器的情况决定是否给其renaming(vk;qk)
			//如果rs2寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order_rs2]_busy) begin
				unsigned int h=reg_las[order_rs2]_reorder;
				if(ROB_s[h]_ready)SLB_s[r]_vk=ROB_s[h]_value;SLB_s[r]_qk=-1;
				else SLB_s[r]_qk=h;
			end
			else SLB_s[r]_vk=reg_las[order_rs2]_reg;SLB_s[r]_qk=-1;
		end
		else SLB_s[r]_qk=-1;
		
		SLB_s[r]_inst=tmp_inst ; SLB_s[r]_ordertype=tmp_ordertype;
		SLB_s[r]_pc=tmp_pc;
		SLB_s[r]_A=order_imm ; SLB_s[r]_reorder=b;
		if(isStore(tmp_ordertype))SLB_s[r]_ready=0;

		//修改register

		if(!isStore(tmp_ordertype)) begin//不为 store指令  (其他都有rd)
			reg[order_rd]_reorder=b;reg[order_rd]_busy=1;
		end
	end
	else  begin// 计算(LUI;AUIPC;ADD;SUB___) or 无条件跳转(BEQ;BNE;BLE___) or 有条件跳转(JAL;JALR)
		
		//找到一个空的RS的位置，r为找到的空的RS的位置
		int r=-1;
		for(i=0;i<`MaxRS;i++)if(!RS_las_s_busy[i]) beginr=i;break;end
		//RS满了，因此取消issue InstructionQueue中的指令
		if(r==-1)return;
		//b为该指令ROB准备存放的位置
		int b=(ROB_las_R+1)%`MaxROB;
		ROB_R=b;ROB_size++;
		//将该指令从Ins_queue删去
		Ins_queue_L=(Ins_queue_las_L+1)%`MaxIns;
		Ins_queue_size--;
		//解码
		Order order=Decode(tmp_inst);

		//修改ROB
		
		ROB_s[b]_inst=tmp_inst; ROB_s[b]_ordertype=tmp_ordertype;
		ROB_s[b]_pc=tmp_pc; ROB_s[b]_jumppc=tmp_jumppc ; ROB_s[b]_isjump=tmp_isjump;
		ROB_s[b]_dest=order_rd ; ROB_s[b]_ready=0;

		//修改RS
		if( (tmp_inst&0x7f)!=0x37&&(tmp_inst&0x7f)!=0x17 && (tmp_inst&0x7f)!=0x6f ) begin// 不为LUI;AUIPC;JAL (有rs1的)
			//根据rs1寄存器的情况决定是否给其renaming(vj;qj)
			//如果rs1寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order_rs1]_busy) begin
				unsigned int h=reg_las[order_rs1]_reorder;
				if(ROB_s[h]_ready) begin
					RS_s[r]_vj=ROB_s[h]_value;RS_s[r]_qj=-1;
				end
				else RS_s[r]_qj=h;
			end
			else RS_s[r]_vj=reg_las[order_rs1]_reg;RS_s[r]_qj=-1;
		end
		else RS_s[r]_qj=-1;

		if( (tmp_inst&0x7f)==0x33 || (tmp_inst&0x7f)==0x63) begin// (ADD__AND) or 有条件跳转  （有rs2的）
			//根据rs2寄存器的情况决定是否给其renaming(vk;qk)
			//如果rs2寄存器上为busy且其最后一次修改对应的ROB位置还未commit，则renaming
			if(reg_las[order_rs2]_busy) begin
				unsigned int h=reg_las[order_rs2]_reorder;
				if(ROB_s[h]_ready)RS_s[r]_vk=ROB_s[h]_value;RS_s[r]_qk=-1;
				else RS_s[r]_qk=h;
			end
			else RS_s[r]_vk=reg_las[order_rs2]_reg;RS_s[r]_qk=-1;
		end
		else RS_s[r]_qk=-1;
		

		RS_s[r]_inst=tmp_inst ; RS_s[r]_ordertype=tmp_ordertype;
		RS_s[r]_pc=tmp_pc ; RS_s[r]_jumppc=tmp_jumppc;
		RS_s[r]_A=order_imm ; RS_s[r]_reorder=b;
		RS_s[r]_busy=1;

		//修改register
		if((tmp_inst&0x7f)!=0x63) begin//不为 有条件跳转  (其他都有rd)
			reg[order_rd]_reorder=b;reg[order_rd]_busy=1;
		end
	end
end
void EX(RS_node tmp;unsigned int &value;unsigned int &jumppc) begin
	if(tmp_ordertype==LUI)value=tmp_A;
	if(tmp_ordertype==AUIPC)value=tmp_pc+tmp_A;

	if(tmp_ordertype==ADD)value=tmp_vj+tmp_vk;
	if(tmp_ordertype==SUB)value=tmp_vj-tmp_vk;
	if(tmp_ordertype==SLL)value=tmp_vj<<(tmp_vk&0x1f);
	if(tmp_ordertype==SLT)value=((int)tmp_vj<(int)tmp_vk)?1:0;
	if(tmp_ordertype==SLTU)value=(tmp_vj<tmp_vk)?1:0;
	if(tmp_ordertype==XOR)value=tmp_vj^tmp_vk;
	if(tmp_ordertype==SRL)value=tmp_vj>>(tmp_vk&0x1f);
	if(tmp_ordertype==SRA)value=(int)tmp_vj>>(tmp_vk&0x1f);
	if(tmp_ordertype==OR)value=tmp_vj|tmp_vk;
	if(tmp_ordertype==AND)value=tmp_vj&tmp_vk;

	if(tmp_ordertype==JALR) begin
		jumppc=(tmp_vj+tmp_A)&(~1);
		value=tmp_pc+4;
	end


	if(tmp_ordertype==ADDI)value=tmp_vj+tmp_A;
	if(tmp_ordertype==SLTI)value=((int)tmp_vj<(int)tmp_A)?1:0;
	if(tmp_ordertype==SLTIU)value=(tmp_vj<tmp_A)?1:0;
	if(tmp_ordertype==XORI)value=tmp_vj^tmp_A;
	if(tmp_ordertype==ORI)value=tmp_vj|tmp_A;
	if(tmp_ordertype==ANDI)value=tmp_vj&tmp_A;
	if(tmp_ordertype==SLLI)value=tmp_vj<<tmp_A;
	if(tmp_ordertype==SRLI)value=tmp_vj>>tmp_A;
	if(tmp_ordertype==SRAI)value=(int)tmp_vj>>tmp_A;
	

	if(tmp_ordertype==JAL) begin
		// printf("imm=%x\n";tmp_A);
		value=tmp_pc+4;
	end


	if(tmp_ordertype==BEQ) begin
		value=(tmp_vj==tmp_vk?1:0);
	end
	if(tmp_ordertype==BNE) begin
		value=(tmp_vj!=tmp_vk?1:0);
	end
	if(tmp_ordertype==BLT) begin
		value=((int)tmp_vj<(int)tmp_vk?1:0);
	end
	if(tmp_ordertype==BGE) begin
		value=((int)tmp_vj>=(int)tmp_vk?1:0);
	end
	if(tmp_ordertype==BLTU) begin
		value=(tmp_vj<tmp_vk?1:0);
	end
	if(tmp_ordertype==BGEU) begin
		value=(tmp_vj>=tmp_vk?1:0);
	end
end
void do_RS() begin
	for(i=0;i<`MaxRS;i++) begin
		if(RS_las_s_busy[i]&&RS_las_s_qj[i]==-1&&RS_las_s_qk[i]==-1) begin
			unsigned value;jumppc;
			EX(RS_las_s;value[i];jumppc);

			// 修改 ROB
			int b=RS_las_s_reorder[i];
			ROB_s[b]_value=value ; ROB_s[b]_ready=1;
			if(RS_las_s_ordertype[i]==JALR)ROB_s[b]_jumppc=jumppc;

			// 修改 RS
			RS_s_busy[i]=0;
			for(int j=0;j<`MaxRS;j++) begin
				if(RS_las_s[j]_busy) begin
					if(RS_las_s[j]_qj==b)RS_s[j]_qj=-1;RS_s[j]_vj=value;
					if(RS_las_s[j]_qk==b)RS_s[j]_qk=-1;RS_s[j]_vk=value;
				end
			end

			// 修改 SLB
			for(int j=0;j<`MaxSLB;j++) begin
				if(SLB_las`_s[j]_qj==b)SLB_s[j]_qj=-1;SLB_s[j]_vj=value;
				if(SLB_las_s[j]_qk==b)SLB_s[j]_qk=-1;SLB_s[j]_vk=value;
			end
			break;
		end
	end
end
void LoadData(SLB_node tmp) begin
	memctrl_data_l_or_s=0;
	if(tmp_ordertype==LB) begin
		unsigned int pos=tmp_vj+tmp_A;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=1;
	end
	if(tmp_ordertype==LH) begin
		unsigned int pos=tmp_vj+tmp_A;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=2;
	end
	if(tmp_ordertype==LW) begin
		unsigned int pos=tmp_vj+tmp_A;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=4;
	end
	if(tmp_ordertype==LBU) begin
		unsigned int pos=tmp_vj+tmp_A;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=1;
	end
	if(tmp_ordertype==LHU) begin
		unsigned int pos=tmp_vj+tmp_A;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=2;
	end
end
void Extend_LoadData(SLB_node tmp;unsigned int data;unsigned int &ans) begin
	//signed:符号位扩展，unsigned：0扩展
	if(tmp_ordertype==LB) begin
		if((ans>>7)&1)ans=data|(0xffffff00);
		else ans=data&(0x000000ff);
	end
	if(tmp_ordertype==LH) begin
		if((ans>>15)&1)ans=data|(0xffff0000);
		else ans=data&(0x0000ffff);
	end
	if(tmp_ordertype==LW)ans=data;
	if(tmp_ordertype==LBU)ans=data&(0x000000ff);
	if(tmp_ordertype==LHU)ans=data&(0x0000ffff);
end
void StoreData(SLB_node tmp) begin
	memctrl_data_l_or_s=1;
	memctrl_data_in=tmp_vk;
	if(tmp_ordertype==SB) begin
		unsigned int pos=tmp_vj+tmp_A;
		// cout<<"SB "<<pos<<endl;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=1;
	end
	if(tmp_ordertype==SH) begin
		unsigned int pos=tmp_vj+tmp_A;
		// cout<<"SH "<<pos<<endl;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=2;
	end
	if(tmp_ordertype==SW) begin
		unsigned int pos=tmp_vj+tmp_A;
		// cout<<"SW "<<pos<<" "<<tmp_vj<<" "<<tmp_A<<endl;
		memctrl_data_addr=pos;
		memctrl_data_remain_cycle=4;
	end
end
void do_SLB() begin
	if(memctrl_las_data_ok) begin
		SLB_is_waiting_data=0;
		int r=SLB_las_L;
		if(isLoad(SLB_las_s[r]_ordertype)) begin
			unsigned int loadvalue;
			Extend_LoadData(SLB_las_s[r];memctrl_las_data_ans;loadvalue);
			// 更改 ROB
			int b=SLB_las_s[r]_reorder;
			ROB_s[b]_value=loadvalue ; ROB_s[b]_ready=1;
			// cout<<"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ loadvalue="<<loadvalue<<endl;

			// 更改 RS
			for(int j=0;j<`MaxRS;j++) begin
				if(RS_las_s[j]_busy) begin
					if(RS_las_s[j]_qj==b)RS_s[j]_qj=-1;RS_s[j]_vj=loadvalue;
					if(RS_las_s[j]_qk==b)RS_s[j]_qk=-1;RS_s[j]_vk=loadvalue;
				end
			end

			// 更改 SLB
			SLB_size--;SLB_L=(SLB_las_L+1)%`MaxSLB;
			SLB_s[SLB_las_L]_qj=-1;SLB_s[SL`B_las_L]_qk=-1;
			for(int j=0;j<`MaxSLB;j++) begin
				if(SLB_las`_s[j]_qj==b)SLB_s[j]_qj=-1;SLB_s[j]_vj=loadvalue;
				if(SLB_las_s[j]_qk==b) begin
					SLB_s[j]_qk=-1;SLB_s[j]_vk=loadvalue;
				end
			end

		end
		else  begin
			// 更改 ROB
			int b=SLB_las_s[r]_reorder;
			ROB_s[b]_ready=1;

			// 更改 SLB
			SLB_size--;SLB_L=(SLB_las_L+1)%`MaxSLB;
			SLB_s[SLB_las_L]_qj=-1;SLB_s[SL`B_las_L]_qk=-1;
		end
	end
	// cout<<"@@@@@@@@"<<SLB_las_L<<endl;
	// cout<<"!!!"<<SLB_las_s[7]_qj<<" "<<SLB_las_s[7]_vj<<endl;

	if(!SLB_las_is_waiting_data&&SLB_las_size) begin
		int r=SLB_las_L;
		if(isLoad(SLB_las_s[r]_ordertype)) begin
			if(SLB_las_s[r]_qj==-1) begin
				LoadData(SLB_las_s[r]);
				SLB_is_waiting_data=1;
			end
		end
		else  begin
			memctrl_data_l_or_s=1;
			if(SLB_las_s[r]_qj==-1&&SLB_las_s[r]_qk==-1&&SLB_las_s[r]_ready) begin
				// cout<<"!!!!"<<r<<" "<<SLB_las_s[r]_vj<<endl; //r=7
				StoreData(SLB_las_s[r]);
				SLB_is_waiting_data=1;
			end
		end
	end
end
void ClearAll() begin
	// cout<<"!!!!  ClearAll  !!!!"<<endl;

	// clear ins_queue
	Ins_queue_L=1;Ins_queue_R=0;Ins_queue_size=0;Ins_queue_is_waiting_ins=0;
	
	// clear RS
	for(i=0;i<`MaxRS;i++) begin
		RS_s_busy[i]=0;
		RS_s_qj[i]=RS_s_qk[i]=-1;
	end

	// clear SLB
	SLB_L=1;SLB_R=0;SLB_size=0;SLB_is_waiting_data=0;
	for(i=0;i<`MaxSLB;i++) begin
		SLB_s_qj[i]=SLB_s_qk[i]=-1;
	end

	// clear ROB
	ROB_L=1;ROB_R=0;ROB_size=0;
	for(i=0;i<`MaxROB;i++)ROB_s_ready[i]=0;

	//clear reg
	for(i=0;i<`MaxReg;i++)reg_busy[i]=0;

	//clear memctrl
	memctrl_ins_remain_cycle=0;memctrl_ins_current_pos=0;memctrl_ins_ok=0;
	memctrl_data_remain_cycle=0;memctrl_data_current_pos=0;memctrl_data_ok=0;

	//clear icache
	for(i=0;i<`MaxICache;i++)icache_valid[i]=0;

	//clear ram
	las_need_return=0;

	// clear flag_END
	flag_END=0;

end
bool Clear_flag=0;
void do_ROB() begin
	if(!ROB_las_size)return;
	int b=ROB_las_L;

	if(isBranch(ROB_las_s[b]_ordertype)) begin
		if(!ROB_las_s[b]_ready)return;
		OKnum++;
		
		// update ROB
		ROB_size--;ROB_L=(ROB_las_L+1)%`MaxROB;

		//JAL必定预测成功
		//让JALR必定预测失败
		if(ROB_las_s[b]_ordertype==JAL) begin

			// update register
			int rd=ROB_las_s[b]_dest;
			reg[rd]_reg=ROB_las_s[b]_value;
			if(reg_las[rd]_busy&&reg_las[rd]_reorder==b)reg[rd]_busy=0;
			// 更改 RS
			for(int j=0;j<`MaxRS;j++) begin
				if(RS_las_s[j]_busy) begin
					if(RS_las_s[j]_qj==b)RS_s[j]_qj=-1;RS_s[j]_vj=ROB_las_s[b]_value;
					if(RS_las_s[j]_qk==b)RS_s[j]_qk=-1;RS_s[j]_vk=ROB_las_s[b]_value;
				end
			end
			// 更改 SLB
			for(int j=0;j<`MaxSLB;j++) begin
				if(SLB_las`_s[j]_qj==b)SLB_s[j]_qj=-1;SLB_s[j]_vj=ROB_las_s[b]_value;
				if(SLB_las_s[j]_qk==b)SLB_s[j]_qk=-1;SLB_s[j]_vk=ROB_las_s[b]_value;
			end
		end
		else  begin
			if(ROB_las_s[b]_ordertype!=JALR)predictTot++;

			if( (ROB_las_s[b]_value^ROB_las_s[b]_isjump)==1 || ROB_las_s[b]_ordertype==JALR) begin//预测失败

				// update BHT
				int x=ROB_las_s[b]_inst&0xfff;
				if(BHT_las[x]_s[0]==0&&BHT_las[x]_s[1]==0)BHT[x]_s[0]=0;BHT[x]_s[1]=1;
				if(BHT_las[x]_s[0]==0&&BHT_las[x]_s[1]==1)BHT[x]_s[0]=1;BHT[x]_s[1]=0;
				if(BHT_las[x]_s[0]==1&&BHT_las[x]_s[1]==0)BHT[x]_s[0]=0;BHT[x]_s[1]=1;
				if(BHT_las[x]_s[0]==1&&BHT_las[x]_s[1]==1)BHT[x]_s[0]=1;BHT[x]_s[1]=0;
				update_BHT_id=x;

				if(ROB_las_s[b]_value)pc=ROB_las_s[b]_jumppc;
				else pc=ROB_las_s[b]_pc+4;
				Clear_flag=1;
				if(ROB_las_s[b]_ordertype==JALR) begin
					
					// update register
					int rd=ROB_las_s[b]_dest;
					reg[rd]_reg=ROB_las_s[b]_value;
					if(reg_las[rd]_busy&&reg_las[rd]_reorder==b)reg[rd]_busy=0;

					// // 更改 RS
					// for(int j=0;j<`MaxRS;j++) begin
					// 	if(RS_las_s[j]_busy) begin
					// 		if(RS_las_s[j]_qj==b)RS_s[j]_qj=-1;RS_s[j]_vj=ROB_las_s[b]_value;
					// 		if(RS_las_s[j]_qk==b)RS_s[j]_qk=-1;RS_s[j]_vk=ROB_las_s[b]_value;
					// 	end
					// end
					// // 更改 SLB
					// for(int j=0;j<`MaxSLB;j++) begin
					// 	if(SLB_las_s[`j]_qj==b)SLB_s[j]_qj=-1;SLB_s[j]_vj=ROB_las_s[b]_value;
					// 	if(SLB_las_s[j]_qk==b)SLB_s[j]_qk=-1;SLB_s[j]_vk=ROB_las_s[b]_value;
					// end
				end
				return;
			end
			else  begin//预测成功
				predictSuccess++;
				// update BHT
				int x=ROB_las_s[b]_inst&0xfff;
				if(BHT_las[x]_s[0]==0&&BHT_las[x]_s[1]==0)BHT[x]_s[0]=0;BHT[x]_s[1]=0;
				if(BHT_las[x]_s[0]==0&&BHT_las[x]_s[1]==1)BHT[x]_s[0]=0;BHT[x]_s[1]=0;
				if(BHT_las[x]_s[0]==1&&BHT_las[x]_s[1]==0)BHT[x]_s[0]=1;BHT[x]_s[1]=1;
				if(BHT_las[x]_s[0]==1&&BHT_las[x]_s[1]==1)BHT[x]_s[0]=1;BHT[x]_s[1]=1;
				update_BHT_id=x;
				return;
			end
		end
	end
	else if(isStore(ROB_las_s[b]_ordertype)) begin
		if(!ROB_las_s[b]_ready) begin
			// update SLB
			for(i=0;i<`MaxSLB;i++) begin
				if(SLB_las`_s_reorder[i]==b) begin
					SLB_s_ready[i]=1;
				end
			end
			return;
		end
		OKnum++;
		// update ROB
		ROB_size--;ROB_L=(ROB_las_L+1)%`MaxROB;
		return;
	end
	else  begin//Load or calc
		if(!ROB_las_s[b]_ready)return;
		OKnum++;
		
		// update ROB
		ROB_size--;ROB_L=(ROB_las_L+1)%`MaxROB;

		// update register
		int rd=ROB_las_s[b]_dest;
		reg[rd]_reg=ROB_las_s[b]_value;
		if(reg_las[rd]_busy&&reg_las[rd]_reorder==b)reg[rd]_busy=0;

		// 更改 RS
		for(int j=0;j<`MaxRS;j++) begin
			if(RS_las_s[j]_busy) begin
				if(RS_las_s[j]_qj==b)RS_s[j]_qj=-1;RS_s[j]_vj=ROB_las_s[b]_value;
				if(RS_las_s[j]_qk==b)RS_s[j]_qk=-1;RS_s[j]_vk=ROB_las_s[b]_value;
			end
		end

		// 更改 SLB
		for(int j=0;j<`MaxSLB;j++) begin
			if(SLB_las`_s[j]_qj==b)SLB_s[j]_qj=-1;SLB_s[j]_vj=ROB_las_s[b]_value;
			if(SLB_las_s[j]_qk==b)SLB_s[j]_qk=-1;SLB_s[j]_vk=ROB_las_s[b]_value;
		end

		return;
	end
end
void update() begin
	pc_las=pc;
	for(i=0;i<`MaxReg;i++)reg_las=reg[i[i]];
	reg_las[0]_reg=0;reg_las[0]_busy=0;//0号寄存器强制为0
	Ins_queue_las=Ins_queue;
	RS_las=RS;
	SLB_las=SLB;
	ROB_las=ROB;
	BHT_las[update_BHT_id]=BHT[update_BHT_id];
	memctrl_las=memctrl;
	icache_las=icache;
	flag_END_las=flag_END;
	Clear_flag=0;
end
void mem_ram(bool en_in;bool r_or_w;unsigned int addr;unsigned char data_in;unsigned char &data_ans) begin//与内存交互 (0:r;1:w)
	// if(en_in) begin
	// 	if(r_or_w==0) begin
	// 		printf("load : get mem[%u] %u\n";addr;mem[addr]);
	// 	end
	// 	else  begin
	// 		printf("store : mem[%u] = %u\n";addr;data_in);
	// 	end
	// end
	if(las_need_return) begin
		data_ans=lasans;
	end
	las_need_return=0;
	if(en_in) begin
		if(r_or_w==0) begin//read
			las_need_return=1;
			lasans=mem[addr];
		end
		else begin//write
			mem[addr]=data_in;
		end
	end
end
void do_memctrl() begin
	bool flag1=!( (1<=memctrl_las_ins_remain_cycle&&memctrl_las_ins_remain_cycle<=3)||memctrl_las_ins_remain_cycle==5 )  
			&&memctrl_las_data_remain_cycle;
	
	if(!(flag1&&memctrl_las_data_l_or_s==0&&memctrl_las_data_remain_cycle==5)&&
		!(flag1&&memctrl_las_data_l_or_s==1&&memctrl_las_data_remain_cycle==1) ) begin
		memctrl_data_ok=0;
	end
	if( ! (!flag1&&memctrl_las_ins_remain_cycle&&memctrl_las_ins_remain_cycle==5) ) begin
		memctrl_ins_ok=0;
	end
	
	if(flag1) begin//ins不在读，且mem可读
		if(memctrl_las_data_l_or_s==0) begin//load
			// unsigned int pos=memctrl_las_data_addr;
			// if(memctrl_las_data_remain_cycle==4) begin
			// 	memctrl_data_ans=(unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8)+((unsigned int)mem[pos+2]<<16)+((unsigned int)mem[pos+3]<<24);
			// end
			// if(memctrl_las_data_remain_cycle==2) begin
			// 	memctrl_data_ans=(unsigned int)mem[pos]+((unsigned int)mem[pos+1]<<8);
			// end
			// if(memctrl_las_data_remain_cycle==1) begin
			// 	memctrl_data_ans=(unsigned int)mem[pos];
			// end
			// memctrl_data_ok=1;
			// memctrl_data_remain_cycle=0;

			
			if(memctrl_las_data_remain_cycle==4) begin
				memctrl_data_remain_cycle=3;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==3) begin
				memctrl_data_remain_cycle=2;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==2) begin
				memctrl_data_remain_cycle=1;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==1) begin
				memctrl_data_remain_cycle=5;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==5) begin
				memctrl_data_remain_cycle=0;
				memctrl_data_current_pos=0;
				memctrl_data_ok=1;
			end

			unsigned char data_in;data_ans;// data_in : meaningless
			bool needvalue=(1<=memctrl_las_data_remain_cycle&&memctrl_las_data_remain_cycle<=4);
			mem_ram(needvalue;0;memctrl_las_data_addr;data_in;data_ans);

			if(memctrl_las_data_current_pos==1) begin
				memctrl_data_ans=(memctrl_las_data_ans&0xffffff00)|data_ans;//[7:0]
			end
			if(memctrl_las_data_current_pos==2) begin
				memctrl_data_ans=(memctrl_las_data_ans&0xffff00ff)|((unsigned int)data_ans<<8);//[15:8]
			end
			if(memctrl_las_data_current_pos==3) begin
				memctrl_data_ans=(memctrl_las_data_ans&0xff00ffff)|((unsigned int)data_ans<<16);//[23:16]
			end
			if(memctrl_las_data_current_pos==4) begin
				memctrl_data_ans=(memctrl_las_data_ans&0x00ffffff)|((unsigned int)data_ans<<24);//[31:24]
			end

			// if(memctrl_data_ok==1) begin
			// 	cout<<memctrl_data_ans<<endl;
			// end
		end
		else  begin//store
			// unsigned int pos=memctrl_las_data_addr;
			// if(memctrl_las_data_remain_cycle==4) begin
			// 	mem[pos]=memctrl_las_data_in&0xff;mem[pos+1]=(memctrl_las_data_in>>8)&0xff;mem[pos+2]=(memctrl_las_data_in>>16)&0xff;mem[pos+3]=(memctrl_las_data_in>>24)&0xff;
			// 	// cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las_data_in<<endl;
			// 	// cout<<pos+1<<" "<<(unsigned int)mem[pos+1]<<" "<<memctrl_las_data_in<<endl;
			// 	// cout<<pos+2<<" "<<(unsigned int)mem[pos+2]<<" "<<memctrl_las_data_in<<endl;
			// 	// cout<<pos+3<<" "<<(unsigned int)mem[pos+3]<<" "<<memctrl_las_data_in<<endl;
			// end
			// if(memctrl_las_data_remain_cycle==2) begin
			// 	mem[pos]=memctrl_las_data_in&0xff;mem[pos+1]=(memctrl_las_data_in>>8)&0xff;
			// 	// cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las_data_in<<endl;
			// 	// cout<<pos+1<<" "<<(unsigned int)mem[pos+1]<<" "<<memctrl_las_data_in<<endl;
			// end
			// if(memctrl_las_data_remain_cycle==1) begin
			// 	mem[pos]=memctrl_las_data_in&0xff;
			// 	// cout<<pos<<" "<<(unsigned int)mem[pos]<<" "<<memctrl_las_data_in<<endl;
			// end
			// memctrl_data_ok=1;
			// memctrl_data_remain_cycle=0;


			unsigned char data_in;data_ans;// data_ans : meaningless

			if(memctrl_las_data_remain_cycle==4) begin
				memctrl_data_remain_cycle=3;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==3) begin
				memctrl_data_remain_cycle=2;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==2) begin
				memctrl_data_remain_cycle=1;
				memctrl_data_current_pos=memctrl_las_data_current_pos+1;
				memctrl_data_addr=memctrl_las_data_addr+1;
			end
			if(memctrl_las_data_remain_cycle==1) begin
				memctrl_data_remain_cycle=0;
				memctrl_data_current_pos=0;
				memctrl_data_ok=1;
			end

			if(memctrl_las_data_current_pos==0) begin
				data_in=(memctrl_las_data_in&0x000000ff);//[7:0]
			end
			if(memctrl_las_data_current_pos==1) begin
				data_in=(memctrl_las_data_in&0x0000ff00)>>8;//[15:8]
			end
			if(memctrl_las_data_current_pos==2) begin
				data_in=(memctrl_las_data_in&0x00ff0000)>>16;//[23:16]
			end
			if(memctrl_las_data_current_pos==3) begin
				data_in=(memctrl_las_data_in&0xff000000)>>24;//[31:24]
			end
			// cout<<memctrl_las_data_addr<<" "<<(unsigned int)data_in<<" "<<memctrl_las_data_in<<endl;
			

			bool needvalue=(1<=memctrl_las_data_remain_cycle&&memctrl_las_data_remain_cycle<=4);
			mem_ram(needvalue;1;memctrl_las_data_addr;data_in;data_ans);
		end
	end
	else if(memctrl_las_ins_remain_cycle) begin
		// unsigned int tmppc=memctrl_las_ins_addr;
		// //memctrl_las_ins_remain_cycle==4
		// memctrl_ins_ans=(unsigned int)mem[tmppc]+((unsigned int)mem[tmppc+1]<<8)+((unsigned int)mem[tmppc+2]<<16)+((unsigned int)mem[tmppc+3]<<24);
		// memctrl_ins_ok=1;
		// memctrl_ins_remain_cycle=0;

		// cout<<memctrl_las_ins_addr<<" "<<memctrl_ins_ans<<endl;

		if(memctrl_las_ins_remain_cycle==4) begin
			memctrl_ins_remain_cycle=3;
			memctrl_ins_current_pos=memctrl_las_ins_current_pos+1;
			memctrl_ins_addr=memctrl_las_ins_addr+1;
		end
		if(memctrl_las_ins_remain_cycle==3) begin
			memctrl_ins_remain_cycle=2;
			memctrl_ins_current_pos=memctrl_las_ins_current_pos+1;
			memctrl_ins_addr=memctrl_las_ins_addr+1;
		end
		if(memctrl_las_ins_remain_cycle==2) begin
			memctrl_ins_remain_cycle=1;
			memctrl_ins_current_pos=memctrl_las_ins_current_pos+1;
			memctrl_ins_addr=memctrl_las_ins_addr+1;
		end
		if(memctrl_las_ins_remain_cycle==1) begin
			memctrl_ins_remain_cycle=5;
			memctrl_ins_current_pos=memctrl_las_ins_current_pos+1;
			memctrl_ins_addr=memctrl_las_ins_addr+1;
		end
		if(memctrl_las_ins_remain_cycle==5) begin
			memctrl_ins_remain_cycle=0;
			memctrl_ins_current_pos=0;
			memctrl_ins_ok=1;
		end

		unsigned char ins_in;ins_ans;// ins_in : meaningless
		bool needvalue=(1<=memctrl_las_ins_remain_cycle&&memctrl_las_ins_remain_cycle<=4);
		mem_ram(needvalue;0;memctrl_las_ins_addr;ins_in;ins_ans);

		if(memctrl_las_ins_current_pos==1) begin
			memctrl_ins_ans=(memctrl_las_ins_ans&0xffffff00)|ins_ans;//[7:0]
		end
		if(memctrl_las_ins_current_pos==2) begin
			memctrl_ins_ans=(memctrl_las_ins_ans&0xffff00ff)|((unsigned int)ins_ans<<8);//[15:8]
		end
		if(memctrl_las_ins_current_pos==3) begin
			memctrl_ins_ans=(memctrl_las_ins_ans&0xff00ffff)|((unsigned int)ins_ans<<16);//[23:16]
		end
		if(memctrl_las_ins_current_pos==4) begin
			memctrl_ins_ans=(memctrl_las_ins_ans&0x00ffffff)|((unsigned int)ins_ans<<24);//[31:24]
		end

		// if(memctrl_ins_ok==1)printf("%x\n";memctrl_ins_ans);
	end
end
int main() begin
	init();
	GetData();
	pc=pc_las=0;
	flag_END=flag_END_las=0;
	while(1) begin
		Clock++;
		// cout<<"clock="<<Clock<<endl;
		do_memctrl();
		Get_ins_to_queue();
		do_ROB();
		do_RS();
		do_SLB();
		do_ins_queue();
		//Get_ins_to_queue()要在do_ROB()前面，因为同时修改了pc，但do_ROB()优先级更高(do_ROB()中的remake)
		//do_ROB()要在do_ins_queue()前面，因为同时修改了reg，但do_ins_queue()优先级更高;且do_ins_queue()中调用了ROB
		if(Clear_flag)ClearAll();
		update();
		// if(OKnum==7000) begin
			// cout<<"Ins_queue_size="<<Ins_queue_las_size<<endl;
			// cout<<"SLB_size="<<SLB_las_size<<endl;
			// cout<<"ROB_size="<<ROB_las_size<<endl;
			// cout<<"ROB_L="<<ROB_las_L<<endl;
			// cout<<"ROB_L type="<<GGG[ROB_las_s[ROB_las_L]_ordertype]<<endl;
			// cout<<OKnum<<endl;
			// cout<<Clock<<endl;
			// for(i=0;i<`MaxReg;i++)cout<<reg_las_reg<<"[i] ";
			// cout<<endl;
		// end
		if(flag_END_las&&Ins_queue_las_size==0&&ROB_las_size==0)break;
		// if(Clock==1000)exit(0);
	end
	printf("%u\n";reg_las[10]_reg&255u);
	// printf("Clock=%d\n";Clock);
	// printf("predict success: %d/%d=%lf\n";predictSuccess;predictTot;predictSuccess*1_0/predictTot);
	return 0;
end