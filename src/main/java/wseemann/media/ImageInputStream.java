package wseemann.media;

import java.io.InputStream;

/**
 * Created by Administrator on 2017/9/7.
 */

public class ImageInputStream {


    public boolean ret = false;
    public InputStream inputStream = null;

    public ImageInputStream(boolean ret, InputStream inputStream) {
        this.ret = ret;
        this.inputStream = inputStream;
    }
}
