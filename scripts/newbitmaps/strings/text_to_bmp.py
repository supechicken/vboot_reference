#!/usr/bin/env python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Render a text file into a bitmap. Files named '*.txt' are small font, those
# nameed '*.TXT' are large font.
#

import codecs
import os
import sys

import Image
import ImageDraw
import ImageFont

# Constant values for rendering properties
IMAGE_BACKGROUND_COLOR = '#607c91'
IMAGE_FONT_BASE = '/usr/share/fonts'
IMAGE_FONT_MAP = {
    '*': 'droid-cros/DroidSans.ttf',
    'ar': 'droid-cros/DroidNaskh-Regular.ttf',
    'iw': 'croscore/Arimo-Regular.ttf',
    'ko': 'droid-cros/DroidSansFallback.ttf',
    'ja': 'droid-cros/DroidSansJapanese.ttf',
    'th': 'droid-cros/DroidSerifThai-Regular.ttf',
    'zh_CN': 'droid-cros/DroidSansFallback.ttf',
    'zh_TW': 'droid-cros/DroidSansFallback.ttf',
}
IMAGE_FONT_SET = {
    'large': {
        'font-size': 40,
        'text-color': 'white',
        'padding': 10,
        'line-spacing': 5,
    },
    '*': {
        'font-size': 22,
        'text-color': '#9ccaec',
        'padding': 3,
        'line-spacing': 3,
    }
}


def die(message):
  """ Prints error message and exit as failure """
  sys.stderr.write("ERROR: %s\n" % message)
  exit(1)


def find_font_file(locale):
  """ Finds appropriate font file for given locale """
  font_file = IMAGE_FONT_MAP.get(locale, IMAGE_FONT_MAP['*'])
  if not font_file.startswith(os.path.sep):
    return os.path.join(IMAGE_FONT_BASE, font_file)
  return font_file


def convert_to_image(input_file, output_file, font_set='*'):
  """ Converts a UTF-8 encoded text message file to image file. """
  # Load message
  bom = unicode(codecs.BOM_UTF8, "utf8")
  with open(input_file, 'r') as input_handle:
    input_messages = input_handle.read().decode('utf-8').lstrip(bom)
    input_messages = input_messages.strip().splitlines()

  # Strip spaces in each line
  input_messages = [message.strip() for message in input_messages]

  # Load fonts
  font_file = find_font_file(os.path.basename(os.path.dirname(input_file)))
  if not os.path.exists(font_file):
    die("Missing font file: %s.\n" % font_file)

  # The output image is...
  # +--------------
  # |            <- padding
  # |  TXT HERE  <---------------\
  # |            <- line_spacing | height
  # | SECOND TEXT<---------------/
  # | ^---------^ width, IMAGE_TEXT_COLOR
  # |^padding, IMAGE_BACKGROUND_COLOR

  # Calculate bounding box
  font = ImageFont.truetype(font_file, IMAGE_FONT_SET[font_set]['font-size'])
  dimension = [font.getsize(message) for message in input_messages]
  width = max((dim[0] for dim in dimension))
  height = sum((dim[1] for dim in dimension))

  # Calculate padding: top/bottom: 1em; double for left/right.
  line_height = int(height / len(input_messages))
  padding_height = IMAGE_FONT_SET[font_set]['padding']
  padding_width = IMAGE_FONT_SET[font_set]['padding']
  line_spacing = IMAGE_FONT_SET[font_set]['line-spacing']
  height += line_spacing * (len(input_messages) - 1)

  # Create image
  im = Image.new('RGB', (width + (padding_width) * 2,
                         height + (padding_height) * 2),
                 IMAGE_BACKGROUND_COLOR)
  draw = ImageDraw.Draw(im)

  # Render text
  text_y = padding_height
  for message in input_messages:
    dim = font.getsize(message)
    text_width = dim[0]
    text_x = padding_width + (width - text_width) / 2
    draw.text((text_x, text_y), message, font=font,
              fill=IMAGE_FONT_SET[font_set]['text-color'])
    text_y += dim[1] + line_spacing

  im.save(output_file)

def main(script, argv):
  global IMAGE_FONT_BASE
  if '--fontdir' in argv:
    fp_index = argv.index('--fontdir')
    IMAGE_FONT_BASE = argv[fp_index + 1]
    # remove --fontdir and its parameter
    argv.pop(fp_index)
    argv.pop(fp_index)

  if len(argv) < 1:
    die('Usage: %s [--fontdir font_base_path] utf8_files...' % script)

  for utf8_file in argv:
    (file_base, file_ext) = os.path.splitext(utf8_file)
    font_set = '*'
    if file_ext == '.TXT':
      font_set = 'large'
    png_file = file_base + '.bmp'
    print 'Converting %s to %s...' % (utf8_file, png_file)
    convert_to_image(utf8_file, png_file, font_set)

if __name__ == '__main__':
  main(sys.argv[0], sys.argv[1:])
