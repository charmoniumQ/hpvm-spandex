from PIL import Image, ImageOps
import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("input", help="input image to be scaled")
parser.add_argument("factor", type=int, help="scaling factor")
args = parser.parse_args()

image_name = args.input
scale = args.factor

original_image = Image.open(image_name+".png")
width, height = original_image.size
print "original size: (" + str(width) + ", " + str(height) + ")"

new_size = (width/scale, height/scale)
new_image = ImageOps.fit(original_image, new_size, Image.ANTIALIAS)
new_width, new_height = new_image.size
print "scaled size: (" + str(new_width) + ", " + str(new_height) + ")"
new_image.save(image_name+"_scaled_"+str(scale)+".png")
