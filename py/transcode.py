#place to keep image bytes
from StringIO import StringIO
#for image operations
from PIL import Image
#for meta data operations
from metacutter import cutFeatures

# transcode each subtile in the result into each format
def Transcode(result, size, formats, formatArgs):
	#a place to keep the subtiles
	tiles = dict([ [f, {}] for f in formats ])
	#for each subtile
	for yy in range(0,size):
		for xx in range(0,size):
			view = result.data[(xx, yy)]
			#so we don't have to palettize twice, in case something supports both gif and png256
			palettized = None
			#save an image for each format that we support
			for f in formats:
				#keep the pil format for this, in case our name is different than PILs
				format = formatArgs[f]['pil_name']
				data = StringIO()
				# little hack to support palettized output for PNG. this really is 
				# needed - on an example image it made it 60% smaller. but it's not
				# an argument to .save(), so have to emulate it manually...
				if 'palette' in formatArgs[f]:
					#if this is the first format the required palettization
					if palettized is None:
						#get the alpha channel
						alpha = view.split()[3]
						#quantize to 255 colors leaving one for transparent
						palettized = view.convert('RGB').convert('P', palette=Image.ADAPTIVE, colors=255)
						#anything less than 1/4 opaque will be transparent (aliased but this is the best we can do)
						imageMask = Image.eval(alpha, lambda a: 255 if a <= 64 else 0)
						#use the 256th color from the palette for all the on-pixels in the mask
						palettized.paste(255, imageMask)
					#save and set the transparency index
					palettized.save(data, format, transparency=255, **(formatArgs[f]))
				#not palettized so just save it
				else:
					view.save(data, format, **(formatArgs[f]))
				#save the image
				tiles[f][(xx, yy)] = data.getvalue()
				#close the buffer
				data.close()
	#hand back the subtiles
	return tiles

#cut the geojson into subtiles and transcode to json string
def TranscodeMeta(features, imageSize, size, mask = None):

	#split the data up into tiles
	if mask is None:
		return cutFeatures(features, imageSize, size, True)
	else:
		#cut up the features into tiles
		metas = []
		for featureCollection in features:
			metas.append(cutFeatures(featureCollection, imageSize, size, True))

		#use the mask to determine per tile which features to use
		meta = dict()
		for location, index in mask.iteritems():
			meta[location] = metas[index][location]

		return meta

		
