import os
import glob
import time
from itertools import cycle
import shutil
from pprint import pprint
#from PIL import Image
import cv2
from time import gmtime, strftime
from random import randint

# output debug message to screen and to file
def debug_out(msg):
	# print to console
	print msg

	# write to file
	with open("debug.log", "a") as myfile:
		myfile.write(msg + "\n")



# move input image from test folder to root dir
def copy_input_img(img_fname):
	dst = "../image.bmp"
	shutil.copyfile(img_fname, dst)
	
	msg = strftime("%H:%M:%S", gmtime()) + "::INFO : Copied new input image : " + img_fname
	debug_out(msg)
	

# move resultant canny_output from root folder to archive dir
def copy_output_img(curr_run_n):
	src = "../out.bmp"
	dst = "images_out/"+ "out_" + str(curr_run_n) + "_" + str(time.time()) + ".bmp"
	shutil.copyfile(src, dst)
	
	msg = strftime("%H:%M:%S", gmtime()) + "::INFO : Copied out.bmp to archive : " + dst
	debug_out(msg)
	
# update the image window
def update_output_image():
	cv2.destroyAllWindows()
	img = cv2.imread("../out.bmp")
	cv2.imshow('image',img)
	cv2.waitKey(30)

# change the seed
def update_seed(curr_run_n):
	f = open ("../seed.txt","r")
	data = f.read()
	
	# change seed
	current_seed = int(data)
	r = randint(1,20) #Inclusive
	new_seed = str(current_seed * r)
	
	f = open ("../seed.txt","w")
	f.write(new_seed)
	
	msg = strftime("%H:%M:%S", gmtime()) + "::INFO : Updated Seed : " + new_seed
	debug_out(msg)
	
###############
# MAIN
###############
debug_out("*****************************************")

	
# get list of images in image directory
list_of_imgs = []
for img_fname in glob.glob("images_in/*.bmp"):
	list_of_imgs.append(img_fname)

pprint(list_of_imgs)
	
img_cycler = cycle(list_of_imgs)

prev_counter = 0
while(1):

	try:
		f = open ("../CannyCounter.txt","r")
		data = f.read()
	
		# get counter value
		curr_counter = int(data)
		msg = strftime("%H:%M:%S", gmtime()) + "::INFO : CannyCounter.txt :  curr_counter = " + str(curr_counter)
		debug_out(msg)
	
	except:
		msg = "Error : CannyCounter.txt read error"
		debug_out(msg)
		curr_counter = prev_counter

	# if counter value is different to previous one, then do the update
	if curr_counter != prev_counter:
		
		# (1) show output image
		update_output_image()		
		
		# (2) move images over
		new_img_fname = next(img_cycler)
		copy_input_img(new_img_fname)
		copy_output_img(curr_counter)
				
		# (3) update seed
		update_seed(curr_counter)
		
		prev_counter = curr_counter
		
	time.sleep(0.5)	# wait x seconds
	