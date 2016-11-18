import fileinput
import glob
import os


for fname in glob.glob("*.log"):    
	
	with open(fname, 'r') as f1:
		content = f1.readlines()
	
	new_content = []
	for every_line in content:
		if len(every_line) > 3:
			#print every_line
			new_content.append(every_line.strip('\n\r'))
	
	with open(fname, 'w') as f2:
		content = f2.writelines('\r'.join(new_content))
	
	
