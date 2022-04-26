// Create a function with the maximum number of parameters, all pointers, to stress the
// quadratic nature of the uniformity analysis.

fn foo(
  p0 : ptr<function, i32>,
  p1 : ptr<function, i32>,
  p2 : ptr<function, i32>,
  p3 : ptr<function, i32>,
  p4 : ptr<function, i32>,
  p5 : ptr<function, i32>,
  p6 : ptr<function, i32>,
  p7 : ptr<function, i32>,
  p8 : ptr<function, i32>,
  p9 : ptr<function, i32>,
  p10 : ptr<function, i32>,
  p11 : ptr<function, i32>,
  p12 : ptr<function, i32>,
  p13 : ptr<function, i32>,
  p14 : ptr<function, i32>,
  p15 : ptr<function, i32>,
  p16 : ptr<function, i32>,
  p17 : ptr<function, i32>,
  p18 : ptr<function, i32>,
  p19 : ptr<function, i32>,
  p20 : ptr<function, i32>,
  p21 : ptr<function, i32>,
  p22 : ptr<function, i32>,
  p23 : ptr<function, i32>,
  p24 : ptr<function, i32>,
  p25 : ptr<function, i32>,
  p26 : ptr<function, i32>,
  p27 : ptr<function, i32>,
  p28 : ptr<function, i32>,
  p29 : ptr<function, i32>,
  p30 : ptr<function, i32>,
  p31 : ptr<function, i32>,
  p32 : ptr<function, i32>,
  p33 : ptr<function, i32>,
  p34 : ptr<function, i32>,
  p35 : ptr<function, i32>,
  p36 : ptr<function, i32>,
  p37 : ptr<function, i32>,
  p38 : ptr<function, i32>,
  p39 : ptr<function, i32>,
  p40 : ptr<function, i32>,
  p41 : ptr<function, i32>,
  p42 : ptr<function, i32>,
  p43 : ptr<function, i32>,
  p44 : ptr<function, i32>,
  p45 : ptr<function, i32>,
  p46 : ptr<function, i32>,
  p47 : ptr<function, i32>,
  p48 : ptr<function, i32>,
  p49 : ptr<function, i32>,
  p50 : ptr<function, i32>,
  p51 : ptr<function, i32>,
  p52 : ptr<function, i32>,
  p53 : ptr<function, i32>,
  p54 : ptr<function, i32>,
  p55 : ptr<function, i32>,
  p56 : ptr<function, i32>,
  p57 : ptr<function, i32>,
  p58 : ptr<function, i32>,
  p59 : ptr<function, i32>,
  p60 : ptr<function, i32>,
  p61 : ptr<function, i32>,
  p62 : ptr<function, i32>,
  p63 : ptr<function, i32>,
  p64 : ptr<function, i32>,
  p65 : ptr<function, i32>,
  p66 : ptr<function, i32>,
  p67 : ptr<function, i32>,
  p68 : ptr<function, i32>,
  p69 : ptr<function, i32>,
  p70 : ptr<function, i32>,
  p71 : ptr<function, i32>,
  p72 : ptr<function, i32>,
  p73 : ptr<function, i32>,
  p74 : ptr<function, i32>,
  p75 : ptr<function, i32>,
  p76 : ptr<function, i32>,
  p77 : ptr<function, i32>,
  p78 : ptr<function, i32>,
  p79 : ptr<function, i32>,
  p80 : ptr<function, i32>,
  p81 : ptr<function, i32>,
  p82 : ptr<function, i32>,
  p83 : ptr<function, i32>,
  p84 : ptr<function, i32>,
  p85 : ptr<function, i32>,
  p86 : ptr<function, i32>,
  p87 : ptr<function, i32>,
  p88 : ptr<function, i32>,
  p89 : ptr<function, i32>,
  p90 : ptr<function, i32>,
  p91 : ptr<function, i32>,
  p92 : ptr<function, i32>,
  p93 : ptr<function, i32>,
  p94 : ptr<function, i32>,
  p95 : ptr<function, i32>,
  p96 : ptr<function, i32>,
  p97 : ptr<function, i32>,
  p98 : ptr<function, i32>,
  p99 : ptr<function, i32>,
  p100 : ptr<function, i32>,
  p101 : ptr<function, i32>,
  p102 : ptr<function, i32>,
  p103 : ptr<function, i32>,
  p104 : ptr<function, i32>,
  p105 : ptr<function, i32>,
  p106 : ptr<function, i32>,
  p107 : ptr<function, i32>,
  p108 : ptr<function, i32>,
  p109 : ptr<function, i32>,
  p110 : ptr<function, i32>,
  p111 : ptr<function, i32>,
  p112 : ptr<function, i32>,
  p113 : ptr<function, i32>,
  p114 : ptr<function, i32>,
  p115 : ptr<function, i32>,
  p116 : ptr<function, i32>,
  p117 : ptr<function, i32>,
  p118 : ptr<function, i32>,
  p119 : ptr<function, i32>,
  p120 : ptr<function, i32>,
  p121 : ptr<function, i32>,
  p122 : ptr<function, i32>,
  p123 : ptr<function, i32>,
  p124 : ptr<function, i32>,
  p125 : ptr<function, i32>,
  p126 : ptr<function, i32>,
  p127 : ptr<function, i32>,
  p128 : ptr<function, i32>,
  p129 : ptr<function, i32>,
  p130 : ptr<function, i32>,
  p131 : ptr<function, i32>,
  p132 : ptr<function, i32>,
  p133 : ptr<function, i32>,
  p134 : ptr<function, i32>,
  p135 : ptr<function, i32>,
  p136 : ptr<function, i32>,
  p137 : ptr<function, i32>,
  p138 : ptr<function, i32>,
  p139 : ptr<function, i32>,
  p140 : ptr<function, i32>,
  p141 : ptr<function, i32>,
  p142 : ptr<function, i32>,
  p143 : ptr<function, i32>,
  p144 : ptr<function, i32>,
  p145 : ptr<function, i32>,
  p146 : ptr<function, i32>,
  p147 : ptr<function, i32>,
  p148 : ptr<function, i32>,
  p149 : ptr<function, i32>,
  p150 : ptr<function, i32>,
  p151 : ptr<function, i32>,
  p152 : ptr<function, i32>,
  p153 : ptr<function, i32>,
  p154 : ptr<function, i32>,
  p155 : ptr<function, i32>,
  p156 : ptr<function, i32>,
  p157 : ptr<function, i32>,
  p158 : ptr<function, i32>,
  p159 : ptr<function, i32>,
  p160 : ptr<function, i32>,
  p161 : ptr<function, i32>,
  p162 : ptr<function, i32>,
  p163 : ptr<function, i32>,
  p164 : ptr<function, i32>,
  p165 : ptr<function, i32>,
  p166 : ptr<function, i32>,
  p167 : ptr<function, i32>,
  p168 : ptr<function, i32>,
  p169 : ptr<function, i32>,
  p170 : ptr<function, i32>,
  p171 : ptr<function, i32>,
  p172 : ptr<function, i32>,
  p173 : ptr<function, i32>,
  p174 : ptr<function, i32>,
  p175 : ptr<function, i32>,
  p176 : ptr<function, i32>,
  p177 : ptr<function, i32>,
  p178 : ptr<function, i32>,
  p179 : ptr<function, i32>,
  p180 : ptr<function, i32>,
  p181 : ptr<function, i32>,
  p182 : ptr<function, i32>,
  p183 : ptr<function, i32>,
  p184 : ptr<function, i32>,
  p185 : ptr<function, i32>,
  p186 : ptr<function, i32>,
  p187 : ptr<function, i32>,
  p188 : ptr<function, i32>,
  p189 : ptr<function, i32>,
  p190 : ptr<function, i32>,
  p191 : ptr<function, i32>,
  p192 : ptr<function, i32>,
  p193 : ptr<function, i32>,
  p194 : ptr<function, i32>,
  p195 : ptr<function, i32>,
  p196 : ptr<function, i32>,
  p197 : ptr<function, i32>,
  p198 : ptr<function, i32>,
  p199 : ptr<function, i32>,
  p200 : ptr<function, i32>,
  p201 : ptr<function, i32>,
  p202 : ptr<function, i32>,
  p203 : ptr<function, i32>,
  p204 : ptr<function, i32>,
  p205 : ptr<function, i32>,
  p206 : ptr<function, i32>,
  p207 : ptr<function, i32>,
  p208 : ptr<function, i32>,
  p209 : ptr<function, i32>,
  p210 : ptr<function, i32>,
  p211 : ptr<function, i32>,
  p212 : ptr<function, i32>,
  p213 : ptr<function, i32>,
  p214 : ptr<function, i32>,
  p215 : ptr<function, i32>,
  p216 : ptr<function, i32>,
  p217 : ptr<function, i32>,
  p218 : ptr<function, i32>,
  p219 : ptr<function, i32>,
  p220 : ptr<function, i32>,
  p221 : ptr<function, i32>,
  p222 : ptr<function, i32>,
  p223 : ptr<function, i32>,
  p224 : ptr<function, i32>,
  p225 : ptr<function, i32>,
  p226 : ptr<function, i32>,
  p227 : ptr<function, i32>,
  p228 : ptr<function, i32>,
  p229 : ptr<function, i32>,
  p230 : ptr<function, i32>,
  p231 : ptr<function, i32>,
  p232 : ptr<function, i32>,
  p233 : ptr<function, i32>,
  p234 : ptr<function, i32>,
  p235 : ptr<function, i32>,
  p236 : ptr<function, i32>,
  p237 : ptr<function, i32>,
  p238 : ptr<function, i32>,
  p239 : ptr<function, i32>,
  p240 : ptr<function, i32>,
  p241 : ptr<function, i32>,
  p242 : ptr<function, i32>,
  p243 : ptr<function, i32>,
  p244 : ptr<function, i32>,
  p245 : ptr<function, i32>,
  p246 : ptr<function, i32>,
  p247 : ptr<function, i32>,
  p248 : ptr<function, i32>,
  p249 : ptr<function, i32>,
  p250 : ptr<function, i32>,
  p251 : ptr<function, i32>,
  p252 : ptr<function, i32>,
  p253 : ptr<function, i32>,
  p254 : ptr<function, i32>,
) {
  *p1 = *p0;
  *p2 = *p1;
  *p3 = *p2;
  *p4 = *p3;
  *p5 = *p4;
  *p6 = *p5;
  *p7 = *p6;
  *p8 = *p7;
  *p9 = *p8;
  *p10 = *p9;
  *p11 = *p10;
  *p12 = *p11;
  *p13 = *p12;
  *p14 = *p13;
  *p15 = *p14;
  *p16 = *p15;
  *p17 = *p16;
  *p18 = *p17;
  *p19 = *p18;
  *p20 = *p19;
  *p21 = *p20;
  *p22 = *p21;
  *p23 = *p22;
  *p24 = *p23;
  *p25 = *p24;
  *p26 = *p25;
  *p27 = *p26;
  *p28 = *p27;
  *p29 = *p28;
  *p30 = *p29;
  *p31 = *p30;
  *p32 = *p31;
  *p33 = *p32;
  *p34 = *p33;
  *p35 = *p34;
  *p36 = *p35;
  *p37 = *p36;
  *p38 = *p37;
  *p39 = *p38;
  *p40 = *p39;
  *p41 = *p40;
  *p42 = *p41;
  *p43 = *p42;
  *p44 = *p43;
  *p45 = *p44;
  *p46 = *p45;
  *p47 = *p46;
  *p48 = *p47;
  *p49 = *p48;
  *p50 = *p49;
  *p51 = *p50;
  *p52 = *p51;
  *p53 = *p52;
  *p54 = *p53;
  *p55 = *p54;
  *p56 = *p55;
  *p57 = *p56;
  *p58 = *p57;
  *p59 = *p58;
  *p60 = *p59;
  *p61 = *p60;
  *p62 = *p61;
  *p63 = *p62;
  *p64 = *p63;
  *p65 = *p64;
  *p66 = *p65;
  *p67 = *p66;
  *p68 = *p67;
  *p69 = *p68;
  *p70 = *p69;
  *p71 = *p70;
  *p72 = *p71;
  *p73 = *p72;
  *p74 = *p73;
  *p75 = *p74;
  *p76 = *p75;
  *p77 = *p76;
  *p78 = *p77;
  *p79 = *p78;
  *p80 = *p79;
  *p81 = *p80;
  *p82 = *p81;
  *p83 = *p82;
  *p84 = *p83;
  *p85 = *p84;
  *p86 = *p85;
  *p87 = *p86;
  *p88 = *p87;
  *p89 = *p88;
  *p90 = *p89;
  *p91 = *p90;
  *p92 = *p91;
  *p93 = *p92;
  *p94 = *p93;
  *p95 = *p94;
  *p96 = *p95;
  *p97 = *p96;
  *p98 = *p97;
  *p99 = *p98;
  *p100 = *p99;
  *p101 = *p100;
  *p102 = *p101;
  *p103 = *p102;
  *p104 = *p103;
  *p105 = *p104;
  *p106 = *p105;
  *p107 = *p106;
  *p108 = *p107;
  *p109 = *p108;
  *p110 = *p109;
  *p111 = *p110;
  *p112 = *p111;
  *p113 = *p112;
  *p114 = *p113;
  *p115 = *p114;
  *p116 = *p115;
  *p117 = *p116;
  *p118 = *p117;
  *p119 = *p118;
  *p120 = *p119;
  *p121 = *p120;
  *p122 = *p121;
  *p123 = *p122;
  *p124 = *p123;
  *p125 = *p124;
  *p126 = *p125;
  *p127 = *p126;
  *p128 = *p127;
  *p129 = *p128;
  *p130 = *p129;
  *p131 = *p130;
  *p132 = *p131;
  *p133 = *p132;
  *p134 = *p133;
  *p135 = *p134;
  *p136 = *p135;
  *p137 = *p136;
  *p138 = *p137;
  *p139 = *p138;
  *p140 = *p139;
  *p141 = *p140;
  *p142 = *p141;
  *p143 = *p142;
  *p144 = *p143;
  *p145 = *p144;
  *p146 = *p145;
  *p147 = *p146;
  *p148 = *p147;
  *p149 = *p148;
  *p150 = *p149;
  *p151 = *p150;
  *p152 = *p151;
  *p153 = *p152;
  *p154 = *p153;
  *p155 = *p154;
  *p156 = *p155;
  *p157 = *p156;
  *p158 = *p157;
  *p159 = *p158;
  *p160 = *p159;
  *p161 = *p160;
  *p162 = *p161;
  *p163 = *p162;
  *p164 = *p163;
  *p165 = *p164;
  *p166 = *p165;
  *p167 = *p166;
  *p168 = *p167;
  *p169 = *p168;
  *p170 = *p169;
  *p171 = *p170;
  *p172 = *p171;
  *p173 = *p172;
  *p174 = *p173;
  *p175 = *p174;
  *p176 = *p175;
  *p177 = *p176;
  *p178 = *p177;
  *p179 = *p178;
  *p180 = *p179;
  *p181 = *p180;
  *p182 = *p181;
  *p183 = *p182;
  *p184 = *p183;
  *p185 = *p184;
  *p186 = *p185;
  *p187 = *p186;
  *p188 = *p187;
  *p189 = *p188;
  *p190 = *p189;
  *p191 = *p190;
  *p192 = *p191;
  *p193 = *p192;
  *p194 = *p193;
  *p195 = *p194;
  *p196 = *p195;
  *p197 = *p196;
  *p198 = *p197;
  *p199 = *p198;
  *p200 = *p199;
  *p201 = *p200;
  *p202 = *p201;
  *p203 = *p202;
  *p204 = *p203;
  *p205 = *p204;
  *p206 = *p205;
  *p207 = *p206;
  *p208 = *p207;
  *p209 = *p208;
  *p210 = *p209;
  *p211 = *p210;
  *p212 = *p211;
  *p213 = *p212;
  *p214 = *p213;
  *p215 = *p214;
  *p216 = *p215;
  *p217 = *p216;
  *p218 = *p217;
  *p219 = *p218;
  *p220 = *p219;
  *p221 = *p220;
  *p222 = *p221;
  *p223 = *p222;
  *p224 = *p223;
  *p225 = *p224;
  *p226 = *p225;
  *p227 = *p226;
  *p228 = *p227;
  *p229 = *p228;
  *p230 = *p229;
  *p231 = *p230;
  *p232 = *p231;
  *p233 = *p232;
  *p234 = *p233;
  *p235 = *p234;
  *p236 = *p235;
  *p237 = *p236;
  *p238 = *p237;
  *p239 = *p238;
  *p240 = *p239;
  *p241 = *p240;
  *p242 = *p241;
  *p243 = *p242;
  *p244 = *p243;
  *p245 = *p244;
  *p246 = *p245;
  *p247 = *p246;
  *p248 = *p247;
  *p249 = *p248;
  *p250 = *p249;
  *p251 = *p250;
  *p252 = *p251;
  *p253 = *p252;
  *p254 = *p253;
}

fn main() {
  var v0 : i32;
  var v1 : i32;
  var v2 : i32;
  var v3 : i32;
  var v4 : i32;
  var v5 : i32;
  var v6 : i32;
  var v7 : i32;
  var v8 : i32;
  var v9 : i32;
  var v10 : i32;
  var v11 : i32;
  var v12 : i32;
  var v13 : i32;
  var v14 : i32;
  var v15 : i32;
  var v16 : i32;
  var v17 : i32;
  var v18 : i32;
  var v19 : i32;
  var v20 : i32;
  var v21 : i32;
  var v22 : i32;
  var v23 : i32;
  var v24 : i32;
  var v25 : i32;
  var v26 : i32;
  var v27 : i32;
  var v28 : i32;
  var v29 : i32;
  var v30 : i32;
  var v31 : i32;
  var v32 : i32;
  var v33 : i32;
  var v34 : i32;
  var v35 : i32;
  var v36 : i32;
  var v37 : i32;
  var v38 : i32;
  var v39 : i32;
  var v40 : i32;
  var v41 : i32;
  var v42 : i32;
  var v43 : i32;
  var v44 : i32;
  var v45 : i32;
  var v46 : i32;
  var v47 : i32;
  var v48 : i32;
  var v49 : i32;
  var v50 : i32;
  var v51 : i32;
  var v52 : i32;
  var v53 : i32;
  var v54 : i32;
  var v55 : i32;
  var v56 : i32;
  var v57 : i32;
  var v58 : i32;
  var v59 : i32;
  var v60 : i32;
  var v61 : i32;
  var v62 : i32;
  var v63 : i32;
  var v64 : i32;
  var v65 : i32;
  var v66 : i32;
  var v67 : i32;
  var v68 : i32;
  var v69 : i32;
  var v70 : i32;
  var v71 : i32;
  var v72 : i32;
  var v73 : i32;
  var v74 : i32;
  var v75 : i32;
  var v76 : i32;
  var v77 : i32;
  var v78 : i32;
  var v79 : i32;
  var v80 : i32;
  var v81 : i32;
  var v82 : i32;
  var v83 : i32;
  var v84 : i32;
  var v85 : i32;
  var v86 : i32;
  var v87 : i32;
  var v88 : i32;
  var v89 : i32;
  var v90 : i32;
  var v91 : i32;
  var v92 : i32;
  var v93 : i32;
  var v94 : i32;
  var v95 : i32;
  var v96 : i32;
  var v97 : i32;
  var v98 : i32;
  var v99 : i32;
  var v100 : i32;
  var v101 : i32;
  var v102 : i32;
  var v103 : i32;
  var v104 : i32;
  var v105 : i32;
  var v106 : i32;
  var v107 : i32;
  var v108 : i32;
  var v109 : i32;
  var v110 : i32;
  var v111 : i32;
  var v112 : i32;
  var v113 : i32;
  var v114 : i32;
  var v115 : i32;
  var v116 : i32;
  var v117 : i32;
  var v118 : i32;
  var v119 : i32;
  var v120 : i32;
  var v121 : i32;
  var v122 : i32;
  var v123 : i32;
  var v124 : i32;
  var v125 : i32;
  var v126 : i32;
  var v127 : i32;
  var v128 : i32;
  var v129 : i32;
  var v130 : i32;
  var v131 : i32;
  var v132 : i32;
  var v133 : i32;
  var v134 : i32;
  var v135 : i32;
  var v136 : i32;
  var v137 : i32;
  var v138 : i32;
  var v139 : i32;
  var v140 : i32;
  var v141 : i32;
  var v142 : i32;
  var v143 : i32;
  var v144 : i32;
  var v145 : i32;
  var v146 : i32;
  var v147 : i32;
  var v148 : i32;
  var v149 : i32;
  var v150 : i32;
  var v151 : i32;
  var v152 : i32;
  var v153 : i32;
  var v154 : i32;
  var v155 : i32;
  var v156 : i32;
  var v157 : i32;
  var v158 : i32;
  var v159 : i32;
  var v160 : i32;
  var v161 : i32;
  var v162 : i32;
  var v163 : i32;
  var v164 : i32;
  var v165 : i32;
  var v166 : i32;
  var v167 : i32;
  var v168 : i32;
  var v169 : i32;
  var v170 : i32;
  var v171 : i32;
  var v172 : i32;
  var v173 : i32;
  var v174 : i32;
  var v175 : i32;
  var v176 : i32;
  var v177 : i32;
  var v178 : i32;
  var v179 : i32;
  var v180 : i32;
  var v181 : i32;
  var v182 : i32;
  var v183 : i32;
  var v184 : i32;
  var v185 : i32;
  var v186 : i32;
  var v187 : i32;
  var v188 : i32;
  var v189 : i32;
  var v190 : i32;
  var v191 : i32;
  var v192 : i32;
  var v193 : i32;
  var v194 : i32;
  var v195 : i32;
  var v196 : i32;
  var v197 : i32;
  var v198 : i32;
  var v199 : i32;
  var v200 : i32;
  var v201 : i32;
  var v202 : i32;
  var v203 : i32;
  var v204 : i32;
  var v205 : i32;
  var v206 : i32;
  var v207 : i32;
  var v208 : i32;
  var v209 : i32;
  var v210 : i32;
  var v211 : i32;
  var v212 : i32;
  var v213 : i32;
  var v214 : i32;
  var v215 : i32;
  var v216 : i32;
  var v217 : i32;
  var v218 : i32;
  var v219 : i32;
  var v220 : i32;
  var v221 : i32;
  var v222 : i32;
  var v223 : i32;
  var v224 : i32;
  var v225 : i32;
  var v226 : i32;
  var v227 : i32;
  var v228 : i32;
  var v229 : i32;
  var v230 : i32;
  var v231 : i32;
  var v232 : i32;
  var v233 : i32;
  var v234 : i32;
  var v235 : i32;
  var v236 : i32;
  var v237 : i32;
  var v238 : i32;
  var v239 : i32;
  var v240 : i32;
  var v241 : i32;
  var v242 : i32;
  var v243 : i32;
  var v244 : i32;
  var v245 : i32;
  var v246 : i32;
  var v247 : i32;
  var v248 : i32;
  var v249 : i32;
  var v250 : i32;
  var v251 : i32;
  var v252 : i32;
  var v253 : i32;
  var v254 : i32;
  v0 = 42;
  foo(
    &v0,
    &v1,
    &v2,
    &v3,
    &v4,
    &v5,
    &v6,
    &v7,
    &v8,
    &v9,
    &v10,
    &v11,
    &v12,
    &v13,
    &v14,
    &v15,
    &v16,
    &v17,
    &v18,
    &v19,
    &v20,
    &v21,
    &v22,
    &v23,
    &v24,
    &v25,
    &v26,
    &v27,
    &v28,
    &v29,
    &v30,
    &v31,
    &v32,
    &v33,
    &v34,
    &v35,
    &v36,
    &v37,
    &v38,
    &v39,
    &v40,
    &v41,
    &v42,
    &v43,
    &v44,
    &v45,
    &v46,
    &v47,
    &v48,
    &v49,
    &v50,
    &v51,
    &v52,
    &v53,
    &v54,
    &v55,
    &v56,
    &v57,
    &v58,
    &v59,
    &v60,
    &v61,
    &v62,
    &v63,
    &v64,
    &v65,
    &v66,
    &v67,
    &v68,
    &v69,
    &v70,
    &v71,
    &v72,
    &v73,
    &v74,
    &v75,
    &v76,
    &v77,
    &v78,
    &v79,
    &v80,
    &v81,
    &v82,
    &v83,
    &v84,
    &v85,
    &v86,
    &v87,
    &v88,
    &v89,
    &v90,
    &v91,
    &v92,
    &v93,
    &v94,
    &v95,
    &v96,
    &v97,
    &v98,
    &v99,
    &v100,
    &v101,
    &v102,
    &v103,
    &v104,
    &v105,
    &v106,
    &v107,
    &v108,
    &v109,
    &v110,
    &v111,
    &v112,
    &v113,
    &v114,
    &v115,
    &v116,
    &v117,
    &v118,
    &v119,
    &v120,
    &v121,
    &v122,
    &v123,
    &v124,
    &v125,
    &v126,
    &v127,
    &v128,
    &v129,
    &v130,
    &v131,
    &v132,
    &v133,
    &v134,
    &v135,
    &v136,
    &v137,
    &v138,
    &v139,
    &v140,
    &v141,
    &v142,
    &v143,
    &v144,
    &v145,
    &v146,
    &v147,
    &v148,
    &v149,
    &v150,
    &v151,
    &v152,
    &v153,
    &v154,
    &v155,
    &v156,
    &v157,
    &v158,
    &v159,
    &v160,
    &v161,
    &v162,
    &v163,
    &v164,
    &v165,
    &v166,
    &v167,
    &v168,
    &v169,
    &v170,
    &v171,
    &v172,
    &v173,
    &v174,
    &v175,
    &v176,
    &v177,
    &v178,
    &v179,
    &v180,
    &v181,
    &v182,
    &v183,
    &v184,
    &v185,
    &v186,
    &v187,
    &v188,
    &v189,
    &v190,
    &v191,
    &v192,
    &v193,
    &v194,
    &v195,
    &v196,
    &v197,
    &v198,
    &v199,
    &v200,
    &v201,
    &v202,
    &v203,
    &v204,
    &v205,
    &v206,
    &v207,
    &v208,
    &v209,
    &v210,
    &v211,
    &v212,
    &v213,
    &v214,
    &v215,
    &v216,
    &v217,
    &v218,
    &v219,
    &v220,
    &v221,
    &v222,
    &v223,
    &v224,
    &v225,
    &v226,
    &v227,
    &v228,
    &v229,
    &v230,
    &v231,
    &v232,
    &v233,
    &v234,
    &v235,
    &v236,
    &v237,
    &v238,
    &v239,
    &v240,
    &v241,
    &v242,
    &v243,
    &v244,
    &v245,
    &v246,
    &v247,
    &v248,
    &v249,
    &v250,
    &v251,
    &v252,
    &v253,
    &v254,
  );
  if (v254 == 0) {
    workgroupBarrier();
  }
}
