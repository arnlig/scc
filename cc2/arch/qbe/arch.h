/* See LICENSE file for copyright and license details. */

enum asmop {
	ASNOP = 0,
	ASSTB,
	ASSTH,
	ASSTW,
	ASSTL,
	ASSTM,
	ASSTS,
	ASSTD,

	ASLDSB,
	ASLDUB,
	ASLDSH,
	ASLDUH,
	ASLDSW,
	ASLDUW,
	ASLDL,
	ASLDS,
	ASLDD,

	ASADDW,
	ASSUBW,
	ASMULW,
	ASMODW,
	ASUMODW,
	ASDIVW,
	ASUDIVW,
	ASSHLW,
	ASSHRW,
	ASUSHRW,
	ASLTW,
	ASULTW,
	ASGTW,
	ASUGTW,
	ASLEW,
	ASULEW,
	ASGEW,
	ASUGEW,
	ASEQW,
	ASNEW,
	ASBANDW,
	ASBORW,
	ASBXORW,

	ASADDL,
	ASSUBL,
	ASMULL,
	ASMODL,
	ASUMODL,
	ASDIVL,
	ASUDIVL,
	ASSHLL,
	ASSHRL,
	ASUSHRL,
	ASLTL,
	ASULTL,
	ASGTL,
	ASUGTL,
	ASLEL,
	ASULEL,
	ASGEL,
	ASUGEL,
	ASEQL,
	ASNEL,
	ASBANDL,
	ASBORL,
	ASBXORL,

	ASADDS,
	ASSUBS,
	ASMULS,
	ASDIVS,
	ASLTS,
	ASGTS,
	ASLES,
	ASGES,
	ASEQS,
	ASNES,

	ASADDD,
	ASSUBD,
	ASMULD,
	ASDIVD,
	ASLTD,
	ASGTD,
	ASLED,
	ASGED,
	ASEQD,
	ASNED,

	ASEXTBW,
	ASUEXTBW,
	ASEXTBL,
	ASUEXTBL,
	ASEXTHW,
	ASUEXTHW,
	ASEXTHL,
	ASUEXTHL,
	ASEXTWL,
	ASUEXTWL,

	ASSTOL,
	ASSTOW,
	ASDTOL,
	ASDTOW,

	ASSWTOD,
	ASSWTOS,
	ASSLTOD,
	ASSLTOS,

	ASEXTS,
	ASTRUNCD,

	ASJMP,
	ASBRANCH,
	ASRET,
	ASCALL,
	ASCALLE,
	ASCALLEX,
	ASPAR,
	ASPARE,
	ASALLOC,
	ASFORM,

	ASCOPYB,
	ASCOPYH,
	ASCOPYW,
	ASCOPYL,
	ASCOPYS,
	ASCOPYD,

	ASVSTAR,
	ASVARG,
};
